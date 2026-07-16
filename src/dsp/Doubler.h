#pragma once

#include <juce_dsp/juce_dsp.h>

#include <array>

// A four-voice vocal doubler: derives a mono sum of the input, feeds it into
// four short, independently modulated delay lines (a click-free detune trick
// via continuous delay-length modulation - a slowly ramping/sawtooth delay
// would be a true pitch shifter but clicks on every reset, which is exactly
// what this avoids), and adds the voices back onto the buffer, each panned to
// its own fixed stereo position (a "per-voice pan" choir spread, not a single
// symmetric L/R pair) scaled by DoubleWidth.
//
// The four voices' modulation LFOs run at different, non-integer-related
// rates and start at different phases so the voices decorrelate over time
// rather than moving in lockstep (a single shared LFO would just sound like
// one voice with a stereo image, not four independent doubles) - this is
// what gives Seraph's doubler a small-choir character rather than a plain
// two-voice chorus.
//
// At amount == 0 the buffer is left bit-exact untouched (the doubler's
// internal delay-line/LFO state still advances, fed from the current input,
// so re-enabling Double mid-stream doesn't start from stale/discontinuous
// state) - this is what keeps Double == 0% part of the plugin's null test.
class Doubler
{
public:
    Doubler() = default;

    // Allocates the delay lines. Must be called before the first process()
    // call, and again whenever sample rate/block size change.
    void prepare (const juce::dsp::ProcessSpec& spec);

    // Clears delay-line and LFO state without deallocating.
    void reset();

    // Amount, 0-100%: gain of the four doubled voices added on top of the
    // existing (centered) signal already in the buffer. 0% is a bit-exact
    // bypass.
    void setAmountProportion (float newAmount01);

    // Detune depth in cents: the peak instantaneous pitch deviation each
    // voice's modulated delay produces.
    void setDetuneCents (float newDetuneCents);

    // Stereo pan spread, 0-100%: 0% keeps all four voices centered (summed
    // equally into both channels), 100% spreads them across the full stereo
    // field at their fixed per-voice pan positions (see voiceConfigs).
    void setWidthProportion (float newWidth01);

    // Processes `block` in place, adding the doubled voices on top of
    // whatever is already there. Mono buffers get all voices summed
    // (unpanned, width has no audible effect, matching the documented v0.1
    // behaviour). A zero-sample block is a safe no-op. No allocation occurs
    // here.
    void process (juce::dsp::AudioBlock<float>& block) noexcept;

private:
    static constexpr int numVoices = 4;

    // Per-voice static configuration: base delay (deliberately offset per
    // voice so they don't start from a correlated comb-filtered
    // relationship with each other), LFO rate (non-integer-related across
    // voices so their modulation drifts in and out of phase rather than
    // locking together), starting LFO phase, and fixed pan position in
    // [-1, +1] (-1 = hard left, 0 = center, +1 = hard right) reached at
    // DoubleWidth == 100%. Voices 0/1 are the original v0.1 outer pair
    // (hard L/R at full width); voices 2/3 are inner voices added in M1 for
    // a fuller small-choir spread.
    struct VoiceConfig
    {
        float baseDelayMs;
        float lfoRateHz;
        double startPhase;
        float panSpread;
    };

    // v0.2.0: base delays re-centered from 13/17/23/29 ms into the 9-24 ms
    // neighborhood of the doubler reference class documented in
    // docs/research-notes.md (tight end ~8-12 ms, outer end ~6-25 ms - see
    // docs/design-brief.md ss2.4). LFO rates/phases/pan roles are unchanged
    // (no reference source published exact 4-voice LFO rates).
    static constexpr std::array<VoiceConfig, numVoices> voiceConfigs { {
        { 9.0f, 0.23f, 0.0, -1.0f },                                     // outer left
        { 24.0f, 0.31f, juce::MathConstants<double>::pi, 1.0f },         // outer right
        { 13.0f, 0.17f, juce::MathConstants<double>::halfPi, -1.0f / 3.0f }, // inner left
        { 19.0f, 0.37f, juce::MathConstants<double>::pi * 1.5, 1.0f / 3.0f } // inner right
    } };

    static constexpr float maxDetuneCents = 50.0f;
    static constexpr double smoothingTimeSeconds = 0.05;

    double sampleRate = 44100.0;

    // Single-channel modulated delay lines: the doubler always derives its
    // voices from a mono sum of the input, so only one channel of state is
    // needed per voice regardless of the host's channel count. Default-
    // constructed here; prepare() calls setMaximumDelayInSamples() on each
    // before any process() call, so the default constructor's initial
    // capacity is never relied upon.
    std::array<juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear>, numVoices> delayLines;

    std::array<double, numVoices> phases {};

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> amountSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> detuneSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> widthSmoothed;

    float lastAmount01 = 0.25f;
    float lastDetuneCents = 10.0f; // v0.2.0 default (was 15), see ParameterLayout.cpp
    float lastWidth01 = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Doubler)
};
