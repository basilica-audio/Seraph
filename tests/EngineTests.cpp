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
    engine.setDeEssWidthProportion (0.4f);
    engine.setDeEssListenEnabled (false);
    engine.setAirDb (0.0f);
    engine.setCompAmountProportion (0.0f);
    engine.setDoubleAmountProportion (0.0f);
    engine.setDoubleDetuneCents (10.0f);
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
        engine.setAirDb (boosted ? 9.0f : 0.0f); // v0.2.0: Air's new max is +9 dB (was +12)
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

    // v0.2.0's wider, gentler shelf (Q ~0.5 vs the old Butterworth ~0.707)
    // spreads the same dB setting's gain across a wider transition band, so
    // this uses a looser (but still clearly discriminating) margin than the
    // old fixed-Q shelf did.
    CHECK (rmsBoosted > rmsNeutral * 1.2);
}

TEST_CASE ("GentleCompressor reduces RMS level of a loud sustained signal", "[dsp][engine][compressor]")
{
    // A steady loud sine, well above the compressor's threshold even at a
    // modest amount - measured over the whole (170 ms @ 48 kHz) block, which
    // is long enough for the 15 ms attack to settle and for reduction to
    // dominate the aggregate RMS, unlike a raw peak comparison which would
    // be dominated by the very first (pre-attack-settled) cycle.
    juce::AudioBuffer<float> loudInput (2, testBlockSize);
    TestHelpers::fillWithSine (loudInput, testSampleRate, 500.0, 0.95f);

    double rmsUncompressed = 0.0;
    double rmsCompressed = 0.0;

    for (const bool compEnabled : { false, true })
    {
        SeraphEngine engine;
        engine.setDeEssAmountProportion (0.0f);
        engine.setAirDb (0.0f);
        engine.setCompAmountProportion (compEnabled ? 1.0f : 0.0f);
        engine.setDoubleAmountProportion (0.0f);
        engine.setMixProportion (1.0f);
        engine.setOutputDb (0.0f);

        const auto spec = makeTestSpec (2);
        engine.prepare (spec);

        juce::AudioBuffer<float> processed;
        processed.makeCopyOf (loudInput);

        juce::dsp::AudioBlock<float> block (processed);
        engine.process (block);

        REQUIRE (TestHelpers::allSamplesFinite (processed));

        (compEnabled ? rmsCompressed : rmsUncompressed) = TestHelpers::rms (processed);
    }

    CHECK (rmsCompressed < rmsUncompressed * 0.9);
}

TEST_CASE ("GentleCompressor at 0% amount is a bit-exact bypass", "[dsp][engine][compressor][null]")
{
    SeraphEngine engine;
    engine.setDeEssAmountProportion (0.0f);
    engine.setAirDb (0.0f);
    engine.setCompAmountProportion (0.0f);
    engine.setDoubleAmountProportion (0.0f);
    engine.setMixProportion (1.0f);
    engine.setOutputDb (0.0f);

    const auto spec = makeTestSpec (2);
    engine.prepare (spec);

    juce::AudioBuffer<float> reference (2, testBlockSize);
    TestHelpers::fillWithSine (reference, testSampleRate, 500.0, 0.9f);

    juce::AudioBuffer<float> processed;
    processed.makeCopyOf (reference);

    juce::dsp::AudioBlock<float> block (processed);
    engine.process (block);

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

TEST_CASE ("De-Ess Listen mode replaces output with the detected sibilance band", "[dsp][engine][deesser][listen]")
{
    constexpr float sibilantFrequencyHz = 7000.0f;

    juce::AudioBuffer<float> sibilantInput (2, testBlockSize);
    TestHelpers::fillWithSine (sibilantInput, testSampleRate, sibilantFrequencyHz, 0.5f);

    SeraphEngine engineNormal;
    engineNormal.setDeEssAmountProportion (0.0f);
    engineNormal.setDeEssListenEnabled (false);
    engineNormal.setDoubleAmountProportion (0.0f);
    engineNormal.setMixProportion (1.0f);

    SeraphEngine engineListen;
    engineListen.setDeEssAmountProportion (0.0f);
    engineListen.setDeEssListenEnabled (true);
    engineListen.setDoubleAmountProportion (0.0f);
    engineListen.setMixProportion (1.0f);

    const auto spec = makeTestSpec (2);
    engineNormal.prepare (spec);
    engineListen.prepare (spec);

    juce::AudioBuffer<float> normalOutput;
    normalOutput.makeCopyOf (sibilantInput);
    juce::dsp::AudioBlock<float> normalBlock (normalOutput);
    engineNormal.process (normalBlock);

    juce::AudioBuffer<float> listenOutput;
    listenOutput.makeCopyOf (sibilantInput);
    juce::dsp::AudioBlock<float> listenBlock (listenOutput);
    engineListen.process (listenBlock);

    REQUIRE (TestHelpers::allSamplesFinite (listenOutput));

    // DeEss amount is 0% (no reduction) on both engines, so "normal" mode is
    // a bit-exact bypass and equals the dry input; listen mode must still
    // differ audibly from the dry input, since it routes the detected band
    // to the output regardless of the reduction amount.
    bool differsFromDry = false;

    for (int channel = 0; channel < listenOutput.getNumChannels() && ! differsFromDry; ++channel)
    {
        const auto* dry = sibilantInput.getReadPointer (channel);
        const auto* listen = listenOutput.getReadPointer (channel);

        for (int i = 0; i < testBlockSize; ++i)
        {
            if (std::abs (dry[i] - listen[i]) > 1.0e-4f)
            {
                differsFromDry = true;
                break;
            }
        }
    }

    CHECK (differsFromDry);
}

TEST_CASE ("Doubler's four voices audibly change the signal and widen the stereo image at full width", "[dsp][engine][doubler]")
{
    juce::AudioBuffer<float> source (2, testBlockSize);
    TestHelpers::fillWithSine (source, testSampleRate, 300.0, 0.4f);

    SeraphEngine engine;
    engine.setDeEssAmountProportion (0.0f);
    engine.setAirDb (0.0f);
    engine.setDoubleAmountProportion (1.0f);
    engine.setDoubleDetuneCents (15.0f);
    engine.setDoubleWidthProportion (1.0f);
    engine.setMixProportion (1.0f);
    engine.setOutputDb (0.0f);

    const auto spec = makeTestSpec (2);
    engine.prepare (spec);

    juce::AudioBuffer<float> buffer;
    buffer.makeCopyOf (source);

    juce::dsp::AudioBlock<float> block (buffer);
    engine.process (block);

    REQUIRE (TestHelpers::allSamplesFinite (buffer));

    // Four voices summed on top of the source (some of them phase-shifted
    // by their base delay relative to a 300 Hz source) should measurably
    // change the aggregate level in at least one direction - i.e. the
    // doubler must not be a silent no-op at Double == 100%. Interference
    // between the delayed voices and the source can just as validly reduce
    // RMS as increase it depending on their relative phase at this
    // frequency, so this only checks that the level actually moved.
    const auto rmsSource = TestHelpers::rms (source);
    const auto rmsDoubled = TestHelpers::rms (buffer);
    CHECK (std::abs (rmsDoubled - rmsSource) > rmsSource * 0.05);

    // Per-voice pan spreads the four voices unevenly across L/R at full
    // width, so the left and right channels should no longer be identical
    // (unlike the mono-sum source, which was identical in both channels).
    bool channelsDiffer = false;

    for (int i = 0; i < testBlockSize && ! channelsDiffer; ++i)
        if (std::abs (buffer.getSample (0, i) - buffer.getSample (1, i)) > 1.0e-4f)
            channelsDiffer = true;

    CHECK (channelsDiffer);
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
