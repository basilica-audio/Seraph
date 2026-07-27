#pragma once

#include <juce_dsp/juce_dsp.h>

#include <vector>

#include "SlidingMinimum.h"

// A single-band, minimum-phase de-esser: detects sibilance energy in a band
// around a configurable center frequency and dynamically reduces it.
//
// Technique ("spectral subtraction" dynamic EQ): a 2nd-order bandpass filter
// isolates the sibilance band from a *copy* of the input; an envelope
// follower measures that band's level and a downward compressor (threshold +
// amount-scaled max reduction, optionally soft-kneed) computes a gain-
// reduction factor for the band. The reduction is applied to the original
// signal by adding back the bandpassed component scaled by (gainFactor - 1):
//
//   output = input + bandpassed * (gainFactor - 1)
//
// which is exactly "input, with the isolated band attenuated by gainFactor"
// (gainFactor == 1 => output == input identically, no residual signal added
// at all - this is what keeps DeEss == 0% a bit-exact bypass for the null
// test in tests/EngineTests.cpp).
//
// v0.3.0 additions (SOTA DSP brief ss3.5), all neutral by default:
//
// * Stereo link - one shared gain driven by max(level_L, level_R), so a
//   hard-panned ess cannot pull the stereo image sideways. Off reproduces
//   v0.2.0's independent per-channel detectors exactly.
//
// * Soft knee - gain reduction interpolated quadratically across a knee of
//   the configured width around the threshold. At width 0 the hard-knee
//   expression from v0.2.0 is evaluated verbatim, so the default stays
//   bit-identical rather than merely equivalent.
//
// * Lookahead - up to 2 ms, so the gain has already started descending by the
//   time the ess arrives.
//
// The lookahead alignment invariant (load-bearing)
// -----------------------------------------------
// The subtraction topology only *attenuates* if the bandpassed term is time-
// aligned with the audio it is subtracted from. With lookahead the output is
//
//   output[n] = x[n - L] + bandpassed[n - L] * (gain[n] - 1)
//
// - BOTH the input and the bandpassed signal are delayed by L, while the
// detector runs on the *undelayed* band (that is what buys the preview time).
// Delaying the band is legitimate because the bandpass is LTI, so delaying
// its output equals filtering the delayed input.
//
// Feeding the undelayed band into a delayed audio path instead - the reading
// "delay the audio, run the detector on the undelayed input" invites - would
// misalign the two by L. Sibilance is noise-like and effectively decorrelated
// at a 2 ms lag, so the "subtraction" would then add roughly 0.8x the band's
// power at maximum reduction: the de-esser would *boost* esses. Both halves
// of that contract (bit-exact delayed passthrough at unity gain, and
// attenuate-never-boost at maximum reduction on decorrelated noise) are
// pinned by tests in tests/DeepDiveTests.cpp.
//
// The gain itself is passed through a sliding minimum over the lookahead
// window before being applied, so it reaches its target before the delayed
// ess does rather than chasing it.
//
// DeEss == 0% remains a bit-exact bypass. With lookahead engaged it is a
// bit-exact *delayed* bypass, so the latency the plugin reports stays
// truthful at every amount setting.
class DeEsser
{
public:
    // Maximum lookahead, in milliseconds (brief ss4).
    static constexpr float maxLookaheadMs = 2.0f;

    DeEsser() = default;

    // Allocates per-channel filter/envelope/lookahead state. Must be called
    // before the first process() call, and again whenever sample rate/channel
    // count/block size change.
    void prepare (const juce::dsp::ProcessSpec& spec);

    // Clears filter, envelope-follower and lookahead state without
    // deallocating.
    void reset();

    // Amount, 0-100%, scales the maximum gain reduction available. 0% is a
    // bit-exact bypass.
    void setAmountProportion (float newAmount01);

    // Center frequency of the sibilance detection/reduction band, Hz.
    void setFrequencyHz (float newFrequencyHz);

    // Detection bandwidth, 0-100% (v0.2.0, docs/design-brief.md ss2.1): maps
    // to the detector bandpass filter's Q, 0% -> maxDetectorQ (narrow,
    // 3.0) through 100% -> minDetectorQ (wide, 0.7). Does not affect the
    // DeEss == 0% bit-exact bypass - only which coefficients feed the
    // detector filter.
    void setWidthProportion (float newWidth01);

    // Sibilance-listen ("solo") mode: when true, process() writes the raw
    // detected sibilance band (the bandpassed detector signal) into `block`
    // instead of the gain-reduced full signal, so DeEssFreq can be tuned by
    // ear independent of the current DeEss amount. False (the default) is a
    // bit-exact no-op on the existing gain-reduction/bypass behaviour below.
    void setListenEnabled (bool shouldListen) noexcept { listenEnabled = shouldListen; }

    // Stereo-linked detection (v0.3.0). False is v0.2.0's behaviour.
    void setLinkEnabled (bool shouldLink) noexcept { linkEnabled = shouldLink; }

    // Soft-knee width in dB around the threshold (v0.3.0). 0 is v0.2.0's
    // hard knee, evaluated through the original expression.
    void setKneeWidthDb (float newKneeWidthDb);

    // Lookahead in milliseconds, 0 to maxLookaheadMs (v0.3.0). Adds
    // round(ms * sr / 1000) samples of reported latency; see
    // getLatencySamples().
    void setLookaheadMs (float newLookaheadMs);

    // Processes `block` in place. A zero-sample block is a safe no-op. No
    // allocation occurs here.
    void process (juce::dsp::AudioBlock<float>& block) noexcept;

    // Reported latency contributed by the lookahead setting, in samples.
    int getLatencySamples() const noexcept { return targetLookaheadSamples; }

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

    // A lookahead change relocates the output in time by up to 2 ms. Rather
    // than let that land as a step, the two read positions are crossfaded
    // over this long - both are available from the same ring buffer, so this
    // costs one extra read per sample and only while a change is in flight.
    static constexpr double lookaheadFadeSeconds = 0.010;

    float computeReductionDb (float overshootDb, float maxReductionDb) const noexcept;

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

    // Lookahead ring buffers, one pair per channel: the input and its
    // bandpassed copy, both read at the same delay so the subtraction stays
    // aligned. Sized in prepare() for maxLookaheadMs at the current sample
    // rate; the delay is a read offset into them, so changing it neither
    // reallocates nor drops samples.
    std::vector<float> inputHistory;
    std::vector<float> bandHistory;
    std::vector<SlidingMinimum> gainMinima;
    int historyLength = 0;
    int historyWriteIndex = 0;
    int numPreparedChannels = 0;

    int targetLookaheadSamples = 0;
    int activeLookaheadSamples = 0;
    int previousLookaheadSamples = 0;
    float lookaheadFadeGain = 1.0f;
    float lookaheadFadeIncrement = 1.0f;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> frequencySmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> amountSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> widthSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> kneeSmoothed;

    float lastFrequencyHz = 7000.0f;
    float lastAmount01 = 0.3f;
    float lastWidth01 = 0.4f;
    float lastKneeWidthDb = 0.0f;
    float lastLookaheadMs = 0.0f;

    float currentGainReductionDb = 0.0f;
    bool listenEnabled = false;
    bool linkEnabled = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeEsser)
};
