#pragma once

#include <juce_dsp/juce_dsp.h>

#include "DeEsser.h"
#include "Doubler.h"

// The complete Seraph signal path, independent of juce::AudioProcessor so it
// can be exercised directly by unit tests without instantiating a full
// plugin (see tests/EngineTests.cpp). Owns all DSP state; every buffer/
// filter/delay line is allocated in prepare() and never reallocated on the
// audio thread.
//
// Signal flow (see docs/architecture.md for the full diagram):
//
//   input -> DeEsser -> Air high-shelf -> Doubler -> Output trim -> Mix -> output
//
// Unlike a distortion/oversampled processor, nothing in this chain adds
// reported host latency: the de-esser is a same-sample minimum-phase
// dynamic-EQ trick, the Air shelf is a plain IIR filter, and the doubler's
// short delay lines are the *effect itself* (a chorus-style detune), not a
// compensation delay - so getLatencySamples() always returns 0 and the
// overall dry/wet Mix is a plain sample-aligned crossfade between the
// original input and the fully processed signal (no DryWetMixer/latency
// compensation dance needed, see OvertureEngine for contrast).
class SeraphEngine
{
public:
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
    void setAirDb (float newAirDb);
    void setDoubleAmountProportion (float newAmount01);
    void setDoubleDetuneCents (float newDetuneCents);
    void setDoubleWidthProportion (float newWidth01);
    void setMixProportion (float newProportion01);
    void setOutputDb (float newOutputDb);

    // Always 0: nothing in this chain adds reported host latency (see the
    // class comment above).
    int getLatencySamples() const noexcept { return 0; }

private:
    static constexpr float airFrequencyHz = 12000.0f; // fixed shelf frequency, within the ~10-16 kHz "Air" register
    static constexpr float airShelfQ = juce::MathConstants<float>::sqrt2 / 2.0f;
    static constexpr double smoothingTimeSeconds = 0.05;

    double sampleRate = 44100.0;

    DeEsser deEsser;
    juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>> airShelf;
    Doubler doubler;
    juce::dsp::Gain<float> outputGain;

    // Pre-allocated capture of the pre-processing ("true dry") signal, used
    // only for the final Mix crossfade - sized in prepare(), never resized
    // on the audio thread. If a host ever sends a block larger than
    // prepare() was told to expect, the crossfade defensively covers only
    // the first `dryBuffer`-sized samples and leaves any overflow tail fully
    // wet rather than reading/writing out of bounds.
    juce::AudioBuffer<float> dryBuffer;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> airDbSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mixSmoothed;

    float lastAirDb = 3.0f;
    float lastMixProportion = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SeraphEngine)
};
