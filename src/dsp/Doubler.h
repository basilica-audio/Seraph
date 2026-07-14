#pragma once

#include <juce_dsp/juce_dsp.h>

// A two-voice vocal doubler: derives a mono sum of the input, feeds it into
// two short, independently modulated delay lines (a click-free detune trick
// via continuous delay-length modulation - a slowly ramping/sawtooth delay
// would be a true pitch shifter but clicks on every reset, which is exactly
// what this avoids), and adds the two voices back onto the buffer panned
// according to width.
//
// The two voices' modulation LFOs run at different, non-integer-related
// rates and start at opposite phase so the voices decorrelate over time
// rather than moving in lockstep (a single shared LFO would just sound like
// one voice with a stereo image, not two independent doubles).
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

    // Amount, 0-100%: gain of the two doubled voices added on top of the
    // existing (centered) signal already in the buffer. 0% is a bit-exact
    // bypass.
    void setAmountProportion (float newAmount01);

    // Detune depth in cents: the peak instantaneous pitch deviation each
    // voice's modulated delay produces.
    void setDetuneCents (float newDetuneCents);

    // Stereo pan spread of the two voices, 0-100%: 0% keeps both voices
    // centered (summed equally into both channels), 100% pans voice A hard
    // left and voice B hard right.
    void setWidthProportion (float newWidth01);

    // Processes `block` in place, adding the doubled voices on top of
    // whatever is already there. Mono buffers get both voices summed
    // (unpanned, width has no audible effect). A zero-sample block is a
    // safe no-op. No allocation occurs here.
    void process (juce::dsp::AudioBlock<float>& block) noexcept;

private:
    // Base delay per voice, deliberately offset so the two voices don't
    // start from a correlated comb-filtered relationship with each other.
    static constexpr float baseDelayMsA = 17.0f;
    static constexpr float baseDelayMsB = 23.0f;

    // LFO rates, deliberately non-integer-related so the two voices'
    // modulation drifts in and out of phase rather than locking together.
    static constexpr float lfoRateHzA = 0.23f;
    static constexpr float lfoRateHzB = 0.31f;

    static constexpr float maxDetuneCents = 50.0f;
    static constexpr double smoothingTimeSeconds = 0.05;

    double sampleRate = 44100.0;

    // Single-channel modulated delay lines: the doubler always derives its
    // two voices from a mono sum of the input, so only one channel of state
    // is needed per voice regardless of the host's channel count.
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLineA { 1 };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLineB { 1 };

    double phaseA = 0.0;
    double phaseB = juce::MathConstants<double>::pi; // opposite starting phase, decorrelates the two voices

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> amountSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> detuneSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> widthSmoothed;

    float lastAmount01 = 0.25f;
    float lastDetuneCents = 15.0f;
    float lastWidth01 = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Doubler)
};
