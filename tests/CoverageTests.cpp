// M1 test-coverage broadening: sample-rate sweeps, mono/stereo bus configs,
// and long-run NaN/Inf stability, in addition to the extreme-parameter
// automation already covered in RobustnessTests.cpp. See the M1 "Broaden
// test coverage" issue.

#include "PluginProcessor.h"
#include "dsp/SeraphEngine.h"
#include "params/ParameterIds.h"
#include "TestHelpers.h"

#include <catch2/catch_test_macros.hpp>

namespace
{
    void setParam (SeraphAudioProcessor& processor, const char* id, float realValue)
    {
        auto* param = processor.apvts.getParameter (id);
        REQUIRE (param != nullptr);
        param->setValueNotifyingHost (param->convertTo0to1 (realValue));
    }
}

TEST_CASE ("Engine null test holds across the full documented sample-rate range", "[dsp][engine][coverage][samplerate]")
{
    static constexpr double sampleRates[] = { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 };
    constexpr int blockSize = 512;

    for (const auto sampleRate : sampleRates)
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
        engine.setMixProportion (1.0f);
        engine.setOutputDb (0.0f);

        juce::dsp::ProcessSpec spec;
        spec.sampleRate = sampleRate;
        spec.maximumBlockSize = static_cast<juce::uint32> (blockSize);
        spec.numChannels = 2;
        engine.prepare (spec);

        REQUIRE (engine.getLatencySamples() == 0);

        juce::AudioBuffer<float> reference (2, blockSize);
        TestHelpers::fillWithSine (reference, sampleRate, 1000.0, 0.5f);

        juce::AudioBuffer<float> processed;
        processed.makeCopyOf (reference);

        juce::dsp::AudioBlock<float> block (processed);
        engine.process (block);

        REQUIRE (TestHelpers::allSamplesFinite (processed));

        constexpr float tolerance = 3.1623e-5f; // 10^(-90/20)

        for (int channel = 0; channel < reference.getNumChannels(); ++channel)
        {
            const auto* refData = reference.getReadPointer (channel);
            const auto* outData = processed.getReadPointer (channel);

            float maxResidual = 0.0f;

            for (int i = 0; i < blockSize; ++i)
                maxResidual = std::max (maxResidual, std::abs (outData[i] - refData[i]));

            CHECK (maxResidual < tolerance);
        }
    }
}

TEST_CASE ("Full processing chain stays finite across the sample-rate range at hot settings", "[dsp][engine][coverage][samplerate]")
{
    static constexpr double sampleRates[] = { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 };
    constexpr int blockSize = 512;

    for (const auto sampleRate : sampleRates)
    {
        SeraphAudioProcessor processor;
        processor.prepareToPlay (sampleRate, blockSize);

        setParam (processor, ParamIDs::deEss, 90.0f);
        setParam (processor, ParamIDs::deEssFreq, 11000.0f);
        setParam (processor, ParamIDs::deEssWidth, 85.0f);
        setParam (processor, ParamIDs::air, 9.0f); // v0.2.0's new max (was +12, tested at 10 before)
        setParam (processor, ParamIDs::comp, 80.0f);
        setParam (processor, ParamIDs::doubleAmount, 90.0f);
        setParam (processor, ParamIDs::doubleDetune, 45.0f);
        setParam (processor, ParamIDs::doubleWidth, 100.0f);
        setParam (processor, ParamIDs::mix, 100.0f);

        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer midi;

        for (int i = 0; i < 8; ++i)
        {
            TestHelpers::fillWithSine (buffer, sampleRate, 9000.0, 0.9f, static_cast<juce::int64> (i) * blockSize);
            CHECK_NOTHROW (processor.processBlock (buffer, midi));
            CHECK (TestHelpers::allSamplesFinite (buffer));
        }

        CHECK (processor.getLatencySamples() == 0);
    }
}

TEST_CASE ("Sample-rate change between prepareToPlay calls does not corrupt state", "[dsp][engine][coverage][samplerate]")
{
    // Simulates a host changing its audio device / sample rate mid-session:
    // prepareToPlay() must be safe to call again with a different rate, and
    // the engine must not retain any stale rate-dependent state (delay-line
    // capacity, filter coefficients) from the previous rate.
    SeraphAudioProcessor processor;

    setParam (processor, ParamIDs::doubleAmount, 70.0f);
    setParam (processor, ParamIDs::doubleDetune, 40.0f);
    setParam (processor, ParamIDs::deEss, 60.0f);

    juce::MidiBuffer midi;

    for (const double sampleRate : { 44100.0, 192000.0, 48000.0, 96000.0 })
    {
        processor.prepareToPlay (sampleRate, 256);

        juce::AudioBuffer<float> buffer (2, 256);
        TestHelpers::fillWithSine (buffer, sampleRate, 2000.0, 0.7f);

        CHECK_NOTHROW (processor.processBlock (buffer, midi));
        CHECK (TestHelpers::allSamplesFinite (buffer));
    }
}

TEST_CASE ("Bus layout: mono and stereo are supported, mismatched in/out and other channel counts are not", "[processor][buses][coverage]")
{
    SeraphAudioProcessor processor;

    auto layoutWith = [] (const juce::AudioChannelSet& in, const juce::AudioChannelSet& out)
    {
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add (in);
        layout.outputBuses.add (out);
        return layout;
    };

    const auto mono = juce::AudioChannelSet::mono();
    const auto stereo = juce::AudioChannelSet::stereo();
    const auto quad = juce::AudioChannelSet::quadraphonic();

    CHECK (processor.isBusesLayoutSupported (layoutWith (stereo, stereo)));
    CHECK (processor.isBusesLayoutSupported (layoutWith (mono, mono)));
    CHECK_FALSE (processor.isBusesLayoutSupported (layoutWith (mono, stereo)));
    CHECK_FALSE (processor.isBusesLayoutSupported (layoutWith (stereo, mono)));
    CHECK_FALSE (processor.isBusesLayoutSupported (layoutWith (quad, quad)));
}

TEST_CASE ("Mono processing is stable and finite across the sample-rate range", "[processor][buses][coverage][samplerate]")
{
    static constexpr double sampleRates[] = { 44100.0, 96000.0, 192000.0 };

    for (const auto sampleRate : sampleRates)
    {
        SeraphAudioProcessor processor;
        processor.prepareToPlay (sampleRate, 256);

        setParam (processor, ParamIDs::deEss, 70.0f);
        setParam (processor, ParamIDs::doubleAmount, 70.0f);
        setParam (processor, ParamIDs::comp, 50.0f);
        setParam (processor, ParamIDs::air, 6.0f);

        juce::AudioBuffer<float> buffer (1, 256);
        juce::MidiBuffer midi;

        for (int i = 0; i < 4; ++i)
        {
            TestHelpers::fillWithSine (buffer, sampleRate, 1500.0, 0.6f, static_cast<juce::int64> (i) * 256);
            CHECK_NOTHROW (processor.processBlock (buffer, midi));
            CHECK (TestHelpers::allSamplesFinite (buffer));
        }
    }
}

TEST_CASE ("Long-run processing at modulated settings never produces NaN/Inf or unbounded growth", "[dsp][engine][coverage][long-run]")
{
    // 4000 blocks of 128 samples @ 48 kHz is ~10.7 s of simulated audio -
    // long enough to catch a slow-building instability (e.g. an envelope or
    // delay-line feedback path that doesn't converge) while staying well
    // under a minute even in a Debug CI build.
    SeraphAudioProcessor processor;
    processor.prepareToPlay (48000.0, 128);

    setParam (processor, ParamIDs::deEss, 55.0f);
    setParam (processor, ParamIDs::deEssFreq, 7500.0f);
    setParam (processor, ParamIDs::deEssWidth, 60.0f);
    setParam (processor, ParamIDs::air, 5.0f);
    setParam (processor, ParamIDs::comp, 40.0f);
    setParam (processor, ParamIDs::doubleAmount, 60.0f);
    setParam (processor, ParamIDs::doubleDetune, 25.0f);
    setParam (processor, ParamIDs::doubleWidth, 80.0f);
    setParam (processor, ParamIDs::mix, 100.0f);

    juce::AudioBuffer<float> buffer (2, 128);
    juce::MidiBuffer midi;

    constexpr int numBlocks = 4000;

    for (int block = 0; block < numBlocks; ++block)
    {
        // A slowly sweeping frequency exercises the de-esser's detector
        // filter and the doubler's modulated delay across a wide spectrum
        // over the run, rather than settling into one steady state.
        const auto frequencyHz = 200.0 + 4000.0 * (0.5 + 0.5 * std::sin (static_cast<double> (block) * 0.01));

        TestHelpers::fillWithSine (buffer, 48000.0, frequencyHz, 0.6f, static_cast<juce::int64> (block) * 128);
        processor.processBlock (buffer, midi);

        REQUIRE (TestHelpers::allSamplesFinite (buffer));
        REQUIRE (TestHelpers::peakAbsolute (buffer) < 100.0f);
    }
}
