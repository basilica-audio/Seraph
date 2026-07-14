#pragma once

#include <juce_dsp/juce_dsp.h>

// A single-band, minimum-phase de-esser: detects sibilance energy in a band
// around a configurable center frequency and dynamically reduces it, without
// adding any latency (no oversampling, no lookahead).
//
// Technique ("spectral subtraction" dynamic EQ): a 2nd-order bandpass filter
// isolates the sibilance band from a *copy* of the input; an envelope
// follower measures that band's level and a hard-knee downward compressor
// (threshold + amount-scaled max reduction) computes a gain-reduction factor
// for the band. The reduction is applied to the original signal by adding
// back the bandpassed component scaled by (gainFactor - 1):
//
//   output = input + bandpassed * (gainFactor - 1)
//
// which is exactly "input, with the isolated band attenuated by gainFactor"
// (gainFactor == 1 => output == input identically, no residual signal added
// at all - this is what keeps DeEss == 0% a bit-exact bypass for the null
// test in tests/EngineTests.cpp).
class DeEsser
{
public:
    DeEsser() = default;

    // Allocates per-channel filter/envelope state. Must be called before the
    // first process() call, and again whenever sample rate/channel count/
    // block size change.
    void prepare (const juce::dsp::ProcessSpec& spec);

    // Clears filter and envelope-follower state without deallocating.
    void reset();

    // Amount, 0-100%, scales the maximum gain reduction available. 0% is a
    // bit-exact bypass.
    void setAmountProportion (float newAmount01);

    // Center frequency of the sibilance detection/reduction band, Hz.
    void setFrequencyHz (float newFrequencyHz);

    // Processes `block` in place. A zero-sample block is a safe no-op. No
    // allocation occurs here.
    void process (juce::dsp::AudioBlock<float>& block) noexcept;

    // Current gain reduction in dB, averaged across channels - exposed for
    // metering/tests, not required for correct audio processing.
    float getCurrentGainReductionDb() const noexcept { return currentGainReductionDb; }

private:
    static constexpr float detectorQ = 1.2f;
    static constexpr float thresholdDb = -28.0f;
    static constexpr float maxReductionRangeDb = 24.0f; // at amount == 100%
    static constexpr double attackTimeSeconds = 0.001;
    static constexpr double releaseTimeSeconds = 0.08;

    double sampleRate = 44100.0;

    // One IIR bandpass filter per channel (not a ProcessorDuplicator): the
    // per-sample combination below needs both the raw input sample and its
    // bandpassed value in the same loop, which the duplicator's block-only
    // `process(context)` API doesn't expose. All channels share the same
    // coefficients object, recomputed once per block from the smoothed
    // frequency (see process()).
    std::vector<juce::dsp::IIR::Filter<float>> detectorFilters;
    juce::dsp::IIR::Coefficients<float>::Ptr detectorCoefficients;

    // Per-channel envelope followers (one-pole attack/release on the
    // squared bandpassed signal), sized in prepare().
    std::vector<float> envelopeState;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> frequencySmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> amountSmoothed;

    float lastFrequencyHz = 7000.0f;
    float lastAmount01 = 0.3f;

    float currentGainReductionDb = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeEsser)
};
