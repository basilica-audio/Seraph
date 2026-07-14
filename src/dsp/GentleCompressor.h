#pragma once

#include <juce_dsp/juce_dsp.h>

// A gentle, broadband, feed-forward downward compressor ("glue" style,
// hand-rolled rather than juce::dsp::Compressor) used to even out level
// before the doubler so all four doubled voices track a consistent main
// signal. Hand-rolled (mirroring DeEsser's structure) rather than wrapping
// juce::dsp::Compressor so CompAmount == 0% can be a bit-exact bypass -
// exactly the same reasoning DeEsser documents for its own hand-rolled
// detector, and needed to keep the plugin's null test bit-exact.
//
// A single Amount knob (0-100%) scales both threshold and ratio together
// from fully transparent (0%) to a gentle maximum (100%, -20 dBFS
// threshold, 3:1 ratio - "glue", not a squashing limiter). No automatic
// makeup gain is applied; use Seraph's Output trim to compensate perceived
// level changes, matching the plugin's minimal-knob philosophy.
//
// Like DeEsser, detection/reduction is per-channel independent (not
// stereo-linked) - documented here rather than left implicit, the same
// acceptable simplification used elsewhere in this signal chain.
class GentleCompressor
{
public:
    GentleCompressor() = default;

    // Allocates per-channel envelope state. Must be called before the first
    // process() call, and again whenever sample rate/channel count/block
    // size change.
    void prepare (const juce::dsp::ProcessSpec& spec);

    // Clears envelope-follower state without deallocating.
    void reset();

    // Amount, 0-100%: scales threshold (0 dBFS down to thresholdMinDb) and
    // ratio (1:1 up to maxRatio) together. 0% is a bit-exact bypass.
    void setAmountProportion (float newAmount01);

    // Processes `block` in place. A zero-sample block is a safe no-op. No
    // allocation occurs here.
    void process (juce::dsp::AudioBlock<float>& block) noexcept;

    // Current gain reduction in dB, averaged across channels - exposed for
    // metering/tests, not required for correct audio processing.
    float getCurrentGainReductionDb() const noexcept { return currentGainReductionDb; }

private:
    static constexpr float thresholdMinDb = -20.0f; // threshold at amount == 100%
    static constexpr float maxRatio = 3.0f;          // ratio at amount == 100%, deliberately gentle
    static constexpr double attackTimeSeconds = 0.015;
    static constexpr double releaseTimeSeconds = 0.15;
    static constexpr double smoothingTimeSeconds = 0.05;

    double sampleRate = 44100.0;

    // Per-channel envelope followers (one-pole attack/release on the
    // squared signal, mirroring DeEsser's detector), sized in prepare().
    std::vector<float> envelopeState;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> amountSmoothed;

    float lastAmount01 = 0.0f;
    float currentGainReductionDb = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GentleCompressor)
};
