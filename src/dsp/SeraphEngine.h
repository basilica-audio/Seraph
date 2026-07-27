#pragma once

#include <juce_dsp/juce_dsp.h>

#include "DeEsser.h"
#include "Doubler.h"
#include "GentleCompressor.h"

// The complete Seraph signal path, independent of juce::AudioProcessor so it
// can be exercised directly by unit tests without instantiating a full
// plugin (see tests/EngineTests.cpp). Owns all DSP state; every buffer/
// filter/delay line is allocated in prepare() and never reallocated on the
// audio thread.
//
// Signal flow (see docs/architecture.md for the full diagram):
//
//   input -> DeEsser -> Air high-shelf -> GentleCompressor -> Doubler -> Output trim -> Mix -> output
//
// Latency (changed in v0.3.0)
// ---------------------------
// Through v0.2.0 this chain reported zero latency unconditionally, and the
// class documented that as an invariant. Two v0.3.0 features deliberately
// break it: the doubler's Shift mode is a phase vocoder, and the de-esser's
// lookahead is a real delay. getLatencySamples() is therefore dynamic, and
// the dry signal used for the final Mix crossfade runs through a compensation
// delay of exactly that length so the crossfade stays sample-aligned in every
// mode. At zero total latency the compensation delay is bypassed and the dry
// path is bit-exact, which is what keeps the existing null tests valid.
//
// Both parameters that can change the reported latency (doubler mode and
// de-esser lookahead) are non-automatable, so a host only ever sees a latency
// change as a deliberate user action.
//
// Sub-block parameter smoothing (v0.3.0)
// --------------------------------------
// process() iterates internally in slices of at most parameterSliceSamples,
// so every smoothed parameter advances several times per host block instead
// of once. Through v0.2.0 a 4096-sample block moved each smoothed value in a
// single step, which is audible as a staircase on fast automation. At static
// settings the smoothed values are converged constants, so the sliced path is
// bit-identical to the unsliced one - the compatibility null tests pin that.
//
// The Air shelf's coefficient recompute deliberately stays at block rate: the
// shelf gain moves on a 50 ms ramp, and recomputing a biquad per 32 samples
// would cost real CPU to chase a value that cannot audibly change that fast.
class SeraphEngine
{
public:
    // Longest run of samples any smoothed parameter is held constant for
    // (brief ss3.7). Also VoiceHumanizer's control-update interval.
    static constexpr int parameterSliceSamples = 32;

    SeraphEngine();

    // Allocates all DSP state. Must be called (and completed) before the
    // first process() call, and again whenever sample rate/block size/
    // channel count change.
    void prepare (const juce::dsp::ProcessSpec& spec);

    // Clears filter/delay-line state without deallocating.
    void reset();

    // Processes `block` in place. `block` must have at most the maximum
    // sample/channel counts declared to prepare(); a zero-sample block is a
    // safe no-op. No allocation occurs here.
    void process (juce::dsp::AudioBlock<float>& block) noexcept;

    // Parameter setters, in real units. Safe to call every block from the
    // audio thread - no allocation/locks.
    void setDeEssAmountProportion (float newAmount01);
    void setDeEssFrequencyHz (float newFrequencyHz);
    void setDeEssWidthProportion (float newWidth01);
    void setDeEssListenEnabled (bool shouldListen);
    void setDeEssLinkEnabled (bool shouldLink);
    void setDeEssKneeDb (float newKneeDb);
    void setDeEssLookaheadMs (float newLookaheadMs);
    void setAirDb (float newAirDb);
    void setAirFrequencyChoice (int choiceIndex);
    void setCompAmountProportion (float newAmount01);
    void setCompLinkEnabled (bool shouldLink);
    void setDoubleAmountProportion (float newAmount01);
    void setDoubleDetuneCents (float newDetuneCents);
    void setDoubleWidthProportion (float newWidth01);
    void setDoubleMode (Doubler::Mode newMode);
    void setDoubleHumanizeProportion (float newHumanize01);
    void setDoubleFormantPreserveEnabled (bool shouldPreserve);
    void setMixProportion (float newProportion01);
    void setOutputDb (float newOutputDb);

    // Total reported host latency: the de-esser's lookahead plus the
    // doubler's mode-dependent latency. Zero in the default configuration.
    int getLatencySamples() const noexcept;

    // The three selectable Air shelf corners, in Hz (brief ss3.8). Index 1
    // (12 kHz) is the fixed constant v0.1/v0.2 always used.
    static constexpr std::array<float, 3> airFrequencyChoicesHz { { 10000.0f, 12000.0f, 15000.0f } };

private:
    // Design frequency for the Air shelf, clamped below Nyquist. 15 kHz is
    // below Nyquist even at 44.1 kHz, so the clamp is a house-rule backstop
    // for exotic rates rather than something the shipped choices hit. Shelf
    // cramping at 15 kHz / 44.1 kHz is accepted and documented; a decramped
    // (matched-Z) design would change the sound at the default setting and
    // needs its own voicing pass.
    float getAirDesignFrequencyHz() const noexcept;

    // v0.2.0: lowered from the Butterworth-Q default (sqrt2/2 ~ 0.707) to a
    // wider, gentler explicit Q so the shelf's transition band starts
    // roughly an octave earlier and reaches full gain roughly an octave
    // later, per the reference class's gentle multi-octave curve - reasoned,
    // not measured (no source publishes the reference unit's filter-design
    // coefficients). See docs/design-brief.md ss2.2/ss5.
    static constexpr float airShelfQ = 0.5f;
    static constexpr double smoothingTimeSeconds = 0.05;

    double sampleRate = 44100.0;

    DeEsser deEsser;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> airShelf;
    GentleCompressor compressor;
    Doubler doubler;
    juce::dsp::Gain<float> outputGain;

    // Pre-allocated capture of the pre-processing ("true dry") signal, used
    // only for the final Mix crossfade - sized in prepare(), never resized
    // on the audio thread. If a host ever sends a block larger than
    // prepare() was told to expect, the crossfade defensively covers only
    // the first `dryBuffer`-sized samples and leaves any overflow tail fully
    // wet rather than reading/writing out of bounds.
    juce::AudioBuffer<float> dryBuffer;

    // Delays the captured dry signal by the current reported latency so the
    // Mix crossfade compares like with like. Sized in prepare() for the
    // worst case any mode/lookahead combination can ask for, so no parameter
    // change ever needs to reallocate. Integer (no-interpolation) delay: the
    // latency is always a whole number of samples.
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> dryCompensationDelay;
    int appliedLatencySamples = 0;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> airDbSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mixSmoothed;

    float lastAirDb = 2.0f; // v0.2.0 default (was 3.0), see ParameterLayout.cpp
    float lastMixProportion = 1.0f;
    int airFrequencyChoice = 1; // 12 kHz, v0.1/v0.2's fixed constant

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SeraphEngine)
};
