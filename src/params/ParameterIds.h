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

    //==========================================================================
    // v0.3.0 additions (SOTA DSP brief ss4). Every one of these defaults to a
    // value that reproduces v0.2.0 behaviour exactly, so sessions and presets
    // written by v0.1.x/v0.2.0 - which carry none of these IDs - load and
    // render bit-identical (tests/StateTests.cpp, tests/EngineTests.cpp).

    // Doubler engine mode, choice: 0 = Classic (the v0.1/v0.2 modulated-delay
    // chorus, default, byte-for-byte unchanged), 1 = Micro (constant-offset
    // dual-head delay-line micropitch, see MicroPitchShifter.h), 2 = Shift
    // (STFT pitch shift with optional formant preservation, see
    // SpectralShifter.h). NOT automatable: Shift reports host latency, so
    // changing the mode changes reported latency (see docs/manual.md).
    inline constexpr auto doubleMode = "doubleMode";

    // Per-voice random-walk humanisation, 0-100%, default 0 (exactly zero
    // offset, so Classic mode stays bit-identical to v0.2.0). Drives slow,
    // deterministic, per-voice drift of timing (+/- 10 ms), pitch (+/- 3
    // cents) and level (+/- 1.5 dB) so stacked voices decorrelate the way
    // real singers do instead of tracking clock-locked LFOs. See
    // VoiceHumanizer.h.
    inline constexpr auto doubleHumanize = "doubleHumanize";

    // Formant preservation for the Shift doubler mode, default on. Has no
    // effect in Classic or Micro mode (neither resamples the spectrum), which
    // is why defaulting it to true is still neutral for old sessions.
    inline constexpr auto doubleFormant = "doubleFormant";

    // Stereo-linked sibilance detection, default off (= v0.2.0's independent
    // per-channel detectors). When on, one shared gain computed from
    // max(level_L, level_R) is applied to both channels, so a hard-panned ess
    // cannot pull the stereo image sideways.
    inline constexpr auto deEssLink = "deEssLink";

    // De-esser knee width in dB, 0-12, default 0 (= v0.2.0's hard knee,
    // bit-identical). Quadratically interpolates gain reduction across the
    // knee region around the fixed -28 dBFS threshold.
    inline constexpr auto deEssKnee = "deEssKnee";

    // De-esser lookahead in ms, 0-2, default 0 (= v0.2.0, no latency). Lets
    // the gain start descending before the ess arrives. NOT automatable: it
    // adds exactly round(ms * sr / 1000) samples of reported host latency.
    inline constexpr auto deEssLookahead = "deEssLookahead";

    // "Air" high-shelf corner frequency, choice: 0 = 10 kHz, 1 = 12 kHz
    // (default - the fixed constant v0.1/v0.2 always used, hence neutral),
    // 2 = 15 kHz.
    inline constexpr auto airFreq = "airFreq";

    // Stereo-linked compressor detection, default off (= v0.2.0's documented
    // per-channel independent detection). When on, one shared envelope
    // computed from max(env_L, env_R) - including the program-dependent
    // auto-release state machine - drives both channels.
    inline constexpr auto compLink = "compLink";
}
