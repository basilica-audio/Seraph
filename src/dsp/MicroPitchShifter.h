#pragma once

#include <juce_dsp/juce_dsp.h>

#include <vector>

// One voice of the doubler's `Micro` mode: a constant-offset micropitch
// shifter built from a single delay line read by two heads whose delay ramps
// linearly, so the Doppler ratio - and therefore the pitch ratio - is
// constant rather than the sinusoidal wobble Classic mode produces (SOTA DSP
// brief ss3.1, H3000 MicroPitch lineage).
//
// Geometry (binding, brief ss3.1 as revised 2026-07-26)
// ----------------------------------------------------
// For a target ratio r = 2^(cents/1200), a delay whose length changes at
// dtau/dt = 1 - r produces exactly that pitch ratio. A finite delay line
// cannot ramp forever, so each head sweeps a fixed range and is periodically
// reset, crossfading with the other head.
//
// The sweep interval is [base, base + sweepRangeMs] - it is deliberately NOT
// centred on the base delay. Centring a 50 ms sweep on the doubler's 9-24 ms
// base delays would need negative (future-reading) delays for every voice, so
// a literal implementation would clamp at the delay line's floor, break the
// constant ratio, and miss the +/- 0.5 cent accuracy target. Sweeping upward
// from the base keeps the instantaneous delay >= base at every sample, so the
// clamp never engages.
//
// Sweep direction follows the sign of the detune: for r > 1 the delay has to
// shrink, so the head runs base + D -> base and wraps back to base + D; for
// r < 1 it runs base -> base + D and wraps to base. The sweep rate is
// f_x = |1 - r| / D (about 0.35 Hz at 30 cents with D = 50 ms).
//
// The two heads are half a sweep apart, and the crossfade gains are arranged
// so that whichever head is wrapping is silent at that instant: head A's gain
// is cos^2(pi*phase) and it wraps at phase 0.5, head B's is sin^2(pi*phase)
// and it wraps at phase 0. The two sum to unity at every phase.
//
// Because the active head sits mid-sweep whenever the other one wraps, the
// mean voice delay is base + D/2 (about 34/49/38/44 ms for the four voices),
// above the 5-30 ms Haas window Classic mode lives in. That is a deliberate,
// documented mode difference: in Micro the comb suppression comes from the
// continuously sweeping delay smearing the comb, and the base delays act as
// per-voice decorrelation offsets rather than strict Haas pre-delays. Micro
// therefore sounds slappier than Classic - Classic itself is untouched.
//
// Reported latency is 0. The brief takes research's stated "report D/2 +
// predelay, *or* 0" alternative and picks 0 on purpose: the inherent ~25 ms
// mean sweep delay is the doubler sound, not a compensable processing delay.
//
// The zero-detune degenerate case
// -------------------------------
// At r == 1 the sweep rate is zero and the geometry above has no meaningful
// answer: the head positions freeze wherever the phase happens to sit. The
// brief specifies that this case degenerates to a pure static delay of `base`
// (it is the analytic reference the Micro null test measures against), so the
// shifter crossfades - over the same 50 ms as every other smoothed parameter -
// between a plain static read at `base` and the swept dual-head output. At a
// settled zero detune the swept path contributes exactly nothing and the
// output is a bit-exact static delay; at any settled non-zero detune the
// static path contributes exactly nothing and the ratio is exact. The
// crossfade only exists to keep the transition between those two regimes
// click-free.
class MicroPitchShifter
{
public:
    // Total sweep range, global across all voices (brief ss3.1). Global rather
    // than per-voice on purpose: f_x is derived from it, and a per-voice D
    // would give every voice a different sideband spacing.
    static constexpr float sweepRangeMs = 50.0f;

    MicroPitchShifter() = default;

    // Allocates the delay line. `newBaseDelayMs` is this voice's sweep floor
    // and decorrelation offset. Must be called before the first
    // processSample() call and whenever the sample rate changes.
    void prepare (double newSampleRate, float newBaseDelayMs);

    // Clears the delay line and returns the sweep to its reset phase without
    // deallocating.
    void reset();

    // Detune in cents for this voice, including any humanisation. Safe to
    // call per sub-block from the audio thread.
    void setDetuneCents (float newDetuneCents) noexcept;

    // Extra delay in milliseconds added to this voice's sweep floor (the
    // humaniser's timing drift). The shifted floor is clamped to >= 0 ms.
    void setBaseDelayOffsetMs (float newOffsetMs) noexcept;

    // Pushes one input sample and returns this voice's shifted output.
    // No allocation occurs here.
    float processSample (float input) noexcept;

    // Sweep reset rate in Hz for the currently configured detune - the
    // spacing of the crossfade sidebands the artifact test bounds. Zero when
    // the detune is zero (the sweep is frozen).
    float getSweepRateHz() const noexcept;

    // Currently configured detune in cents, after humanisation.
    float getDetuneCents() const noexcept { return detuneCents; }

private:
    // Reads the delay line `delaySamples` in the past using cubic
    // Catmull-Rom interpolation (brief ss3.1). Catmull-Rom rather than the
    // Classic path's linear interpolation because a constant-ratio sweep
    // spends every sample at a different fractional offset, where linear
    // interpolation's frequency-dependent loss would modulate the timbre at
    // the sweep rate.
    float readCubic (float delaySamples) const noexcept;

    static constexpr double smoothingTimeSeconds = 0.05;
    static constexpr float maxDelayLineMs = 160.0f; // 24 ms max base + 50 ms sweep + 10 ms humanise + headroom
    static constexpr float minReadDelaySamples = 2.0f; // Catmull-Rom needs one older tap either side

    double sampleRate = 44100.0;
    double samplesPerMillisecond = 44.1;
    float baseDelayMs = 0.0f;
    float baseDelayOffsetMs = 0.0f;
    float detuneCents = 0.0f;

    float ratio = 1.0f;
    float sweepRateHz = 0.0f;
    double sweepPhase = 0.0;      // [0, 1)
    double phaseIncrement = 0.0;

    // 0 = pure static read at the sweep floor, 1 = pure dual-head sweep.
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> sweepEngagement;

    std::vector<float> buffer;
    int bufferSize = 0;
    int writeIndex = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MicroPitchShifter)
};
