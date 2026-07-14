#include "dsp/SeraphEngine.h"
#include "TestHelpers.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>

namespace
{
    constexpr double testSampleRate = 48000.0;
    constexpr int testBlockSize = 8192;

    juce::dsp::ProcessSpec makeTestSpec (int numChannels)
    {
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = testSampleRate;
        spec.maximumBlockSize = static_cast<juce::uint32> (testBlockSize);
        spec.numChannels = static_cast<juce::uint32> (numChannels);
        return spec;
    }
}

TEST_CASE ("Engine null test: neutral settings null against the input", "[dsp][engine][null]")
{
    SeraphEngine engine;

    engine.setDeEssAmountProportion (0.0f);
    engine.setDeEssFrequencyHz (7000.0f);
    engine.setAirDb (0.0f);
    engine.setDoubleAmountProportion (0.0f);
    engine.setDoubleDetuneCents (15.0f);
    engine.setDoubleWidthProportion (1.0f);
    engine.setMixProportion (1.0f); // fully wet - the "wet" chain itself must equal the input here
    engine.setOutputDb (0.0f);

    const auto spec = makeTestSpec (2);
    engine.prepare (spec);

    REQUIRE (engine.getLatencySamples() == 0);

    juce::AudioBuffer<float> reference (2, testBlockSize);
    TestHelpers::fillWithSine (reference, testSampleRate, 1000.0, 0.5f);

    juce::AudioBuffer<float> processed;
    processed.makeCopyOf (reference);

    juce::dsp::AudioBlock<float> block (processed);
    engine.process (block);

    // < -90 dBFS residual, in linear amplitude.
    constexpr float tolerance = 3.1623e-5f; // 10^(-90/20)

    for (int channel = 0; channel < reference.getNumChannels(); ++channel)
    {
        const auto* refData = reference.getReadPointer (channel);
        const auto* outData = processed.getReadPointer (channel);

        float maxResidual = 0.0f;

        for (int i = 0; i < testBlockSize; ++i)
            maxResidual = std::max (maxResidual, std::abs (outData[i] - refData[i]));

        CHECK (maxResidual < tolerance);
    }
}

TEST_CASE ("De-esser reduces energy of a sibilant-like input", "[dsp][engine][deesser]")
{
    constexpr float sibilantFrequencyHz = 7000.0f;

    juce::AudioBuffer<float> sibilantInput (2, testBlockSize);
    TestHelpers::fillWithSine (sibilantInput, testSampleRate, sibilantFrequencyHz, 0.5f);

    double rmsWithoutDeEss = 0.0;
    double rmsWithDeEss = 0.0;

    for (const bool deEssEnabled : { false, true })
    {
        SeraphEngine engine;
        engine.setDeEssAmountProportion (deEssEnabled ? 1.0f : 0.0f);
        engine.setDeEssFrequencyHz (sibilantFrequencyHz);
        engine.setAirDb (0.0f);
        engine.setDoubleAmountProportion (0.0f);
        engine.setMixProportion (1.0f);
        engine.setOutputDb (0.0f);

        const auto spec = makeTestSpec (2);
        engine.prepare (spec);

        juce::AudioBuffer<float> processed;
        processed.makeCopyOf (sibilantInput);

        juce::dsp::AudioBlock<float> block (processed);
        engine.process (block);

        REQUIRE (TestHelpers::allSamplesFinite (processed));

        (deEssEnabled ? rmsWithDeEss : rmsWithoutDeEss) = TestHelpers::rms (processed);
    }

    CHECK (rmsWithDeEss < rmsWithoutDeEss * 0.7);
}

TEST_CASE ("Air high-shelf boosts high-frequency content", "[dsp][engine][air]")
{
    constexpr float highFrequencyHz = 13000.0f; // within the ~10-16 kHz Air register

    juce::AudioBuffer<float> highFreqInput (2, testBlockSize);
    TestHelpers::fillWithSine (highFreqInput, testSampleRate, highFrequencyHz, 0.1f);

    double rmsNeutral = 0.0;
    double rmsBoosted = 0.0;

    for (const bool boosted : { false, true })
    {
        SeraphEngine engine;
        engine.setDeEssAmountProportion (0.0f);
        engine.setAirDb (boosted ? 12.0f : 0.0f);
        engine.setDoubleAmountProportion (0.0f);
        engine.setMixProportion (1.0f);
        engine.setOutputDb (0.0f);

        const auto spec = makeTestSpec (2);
        engine.prepare (spec);

        juce::AudioBuffer<float> processed;
        processed.makeCopyOf (highFreqInput);

        juce::dsp::AudioBlock<float> block (processed);
        engine.process (block);

        REQUIRE (TestHelpers::allSamplesFinite (processed));

        (boosted ? rmsBoosted : rmsNeutral) = TestHelpers::rms (processed);
    }

    CHECK (rmsBoosted > rmsNeutral * 1.5);
}

TEST_CASE ("Engine reset() clears filter/delay-line state without crashing", "[dsp][engine]")
{
    SeraphEngine engine;
    engine.setDeEssAmountProportion (0.8f);
    engine.setDoubleAmountProportion (0.8f);
    engine.setMixProportion (1.0f);

    const auto spec = makeTestSpec (2);
    engine.prepare (spec);

    juce::AudioBuffer<float> buffer (2, testBlockSize);
    TestHelpers::fillWithSine (buffer, testSampleRate, 1000.0, 0.9f);

    juce::dsp::AudioBlock<float> block (buffer);
    engine.process (block);

    CHECK_NOTHROW (engine.reset());
    CHECK (TestHelpers::allSamplesFinite (buffer));

    TestHelpers::fillWithSine (buffer, testSampleRate, 1000.0, 0.9f);
    CHECK_NOTHROW (engine.process (block));
    CHECK (TestHelpers::allSamplesFinite (buffer));
}
