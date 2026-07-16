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

    // Detection bandwidth, 0-100% (v0.2.0, docs/design-brief.md ss2.1): maps
    // to the detector bandpass filter's Q, 0% -> minDetectorQ (narrow,
    // 3.0) through 100% -> maxDetectorQ (wide, 0.7). Does not affect the
    // DeEss == 0% bit-exact bypass - only which coefficients feed the
    // detector filter.
    void setWidthProportion (float newWidth01);

    // Sibilance-listen ("solo") mode: when true, process() writes the raw
    // detected sibilance band (the bandpassed detector signal) into `block`
    // instead of the gain-reduced full signal, so DeEssFreq can be tuned by
    // ear independent of the current DeEss amount. False (the default) is a
    // bit-exact no-op on the existing gain-reduction/bypass behaviour below.
    void setListenEnabled (bool shouldListen) noexcept { listenEnabled = shouldListen; }

    // Processes `block` in place. A zero-sample block is a safe no-op. No
    // allocation occurs here.
    void process (juce::dsp::AudioBlock<float>& block) noexcept;

    // Current gain reduction in dB, averaged across channels - exposed for
    // metering/tests, not required for correct audio processing.
    float getCurrentGainReductionDb() const noexcept { return currentGainReductionDb; }

private:
    // v0.2.0: the old fixed detectorQ = 1.2 constant is replaced by
    // DeEssWidth, a user-facing control mapped linearly between these two
    // extremes (see docs/design-brief.md ss2.1 and ss5's honesty note - this
    // Q range is reasoned, not sourced to an exact figure from either
    // reference de-esser's manual). At the parameter's own default (40%),
    // this reproduces a Q reasonably close to (not identical to) v1's fixed
    // 1.2, documented here rather than left implicit.
    static constexpr float minDetectorQ = 0.7f;  // DeEssWidth == 100% (wide)
    static constexpr float maxDetectorQ = 3.0f;  // DeEssWidth == 0% (narrow)
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
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> widthSmoothed;

    float lastFrequencyHz = 7000.0f;
    float lastAmount01 = 0.3f;
    float lastWidth01 = 0.4f;

    float currentGainReductionDb = 0.0f;
    bool listenEnabled = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeEsser)
};
