#include "PluginProcessor.h"
#include "params/ParameterIds.h"
#include "TestHelpers.h"

#include <catch2/catch_test_macros.hpp>

#include <limits>
#include <random>

namespace
{
    void setParam (SeraphAudioProcessor& processor, const char* id, float realValue)
    {
        auto* param = processor.apvts.getParameter (id);
        REQUIRE (param != nullptr);
        param->setValueNotifyingHost (param->convertTo0to1 (realValue));
    }
}

TEST_CASE ("Silence produces silence (and no NaN/Inf)", "[robustness]")
{
    SeraphAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    setParam (processor, ParamIDs::deEss, 80.0f);
    setParam (processor, ParamIDs::deEssWidth, 90.0f);
    setParam (processor, ParamIDs::air, 9.0f); // v0.2.0's new max (was +12)
    setParam (processor, ParamIDs::doubleAmount, 80.0f);
    setParam (processor, ParamIDs::mix, 100.0f);

    juce::AudioBuffer<float> buffer (2, 512);
    buffer.clear();

    juce::MidiBuffer midi;

    for (int i = 0; i < 8; ++i)
        CHECK_NOTHROW (processor.processBlock (buffer, midi));

    CHECK (TestHelpers::allSamplesFinite (buffer));
}

TEST_CASE ("Full-scale sibilant-like input at maximum settings produces no NaN/Inf", "[robustness]")
{
    SeraphAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    setParam (processor, ParamIDs::deEss, 100.0f);
    setParam (processor, ParamIDs::deEssFreq, 9000.0f);
    setParam (processor, ParamIDs::deEssWidth, 100.0f);
    setParam (processor, ParamIDs::comp, 100.0f);
    setParam (processor, ParamIDs::air, 9.0f); // v0.2.0's new max (was +12)
    setParam (processor, ParamIDs::doubleAmount, 100.0f);
    setParam (processor, ParamIDs::doubleDetune, 50.0f);
    setParam (processor, ParamIDs::doubleWidth, 100.0f);
    setParam (processor, ParamIDs::output, 24.0f);
    setParam (processor, ParamIDs::mix, 100.0f);

    // Each iteration refills the buffer with fresh, phase-continuous input
    // (as a host would present a new block of real audio) rather than
    // reprocessing the previous iteration's already-processed output: unlike
    // overture's oversampled tanh clipper, nothing in Seraph's chain is a
    // saturating nonlinearity, so repeatedly re-feeding a linear gain chain
    // (Air +9 dB * Doubler sum * Output +24 dB) its own output would
    // compound exponentially every iteration - that is a test-construction
    // artifact, not a representative "full-scale input" scenario.
    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;

    for (int i = 0; i < 16; ++i)
    {
        TestHelpers::fillWithSine (buffer, 48000.0, 8000.0, 1.0f, static_cast<juce::int64> (i) * 512);
        CHECK_NOTHROW (processor.processBlock (buffer, midi));
        CHECK (TestHelpers::allSamplesFinite (buffer));
    }

    // Generous sane bound for a linear gain chain at extreme (but valid)
    // parameter combinations (Air +9 dB, Double 100% summed both voices,
    // Output +24 dB stacked on a full-scale input) - not "finite" alone,
    // but not the exponential blow-up an accidental feedback loop would
    // produce either.
    CHECK (TestHelpers::peakAbsolute (buffer) < 1000.0f);
}

TEST_CASE ("Denormal-range input produces no NaN/Inf output", "[robustness]")
{
    SeraphAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    setParam (processor, ParamIDs::deEss, 50.0f);
    setParam (processor, ParamIDs::doubleAmount, 50.0f);
    setParam (processor, ParamIDs::mix, 100.0f);

    constexpr int numSamples = 512;
    juce::AudioBuffer<float> buffer (2, numSamples);

    const auto denormalValue = std::numeric_limits<float>::denorm_min() * 4.0f;

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        auto* data = buffer.getWritePointer (channel);

        for (int sample = 0; sample < numSamples; ++sample)
            data[sample] = (sample % 2 == 0) ? denormalValue : -denormalValue;
    }

    juce::MidiBuffer midi;

    for (int i = 0; i < 8; ++i)
        CHECK_NOTHROW (processor.processBlock (buffer, midi));

    CHECK (TestHelpers::allSamplesFinite (buffer));
}

TEST_CASE ("Zero-sample buffer does not crash processBlock", "[robustness]")
{
    SeraphAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    juce::AudioBuffer<float> buffer (2, 0);
    juce::MidiBuffer midi;

    CHECK_NOTHROW (processor.processBlock (buffer, midi));
    CHECK (buffer.getNumSamples() == 0);
}

TEST_CASE ("Mono buffer does not crash processBlock", "[robustness]")
{
    SeraphAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    setParam (processor, ParamIDs::doubleAmount, 60.0f);
    setParam (processor, ParamIDs::deEss, 40.0f);

    juce::AudioBuffer<float> buffer (1, 512);
    TestHelpers::fillWithSine (buffer, 48000.0, 1000.0, 0.6f);
    juce::MidiBuffer midi;

    CHECK_NOTHROW (processor.processBlock (buffer, midi));
    CHECK (TestHelpers::allSamplesFinite (buffer));
}

TEST_CASE ("Extreme parameter values at both range edges produce no NaN/Inf", "[robustness]")
{
    SeraphAudioProcessor processor;
    processor.prepareToPlay (44100.0, 256);

    juce::AudioBuffer<float> buffer (2, 256);
    juce::MidiBuffer midi;

    for (bool useMinimum : { true, false })
    {
        setParam (processor, ParamIDs::deEss, useMinimum ? 0.0f : 100.0f);
        setParam (processor, ParamIDs::deEssFreq, useMinimum ? 3000.0f : 12000.0f);
        setParam (processor, ParamIDs::deEssWidth, useMinimum ? 0.0f : 100.0f);
        setParam (processor, ParamIDs::comp, useMinimum ? 0.0f : 100.0f);
        setParam (processor, ParamIDs::air, useMinimum ? -6.0f : 9.0f); // v0.2.0 range (was -12/+12)
        setParam (processor, ParamIDs::doubleAmount, useMinimum ? 0.0f : 100.0f);
        setParam (processor, ParamIDs::doubleDetune, useMinimum ? 0.0f : 50.0f);
        setParam (processor, ParamIDs::doubleWidth, useMinimum ? 0.0f : 100.0f);
        setParam (processor, ParamIDs::output, useMinimum ? -24.0f : 24.0f);
        setParam (processor, ParamIDs::mix, useMinimum ? 0.0f : 100.0f);

        for (const bool listenEnabled : { false, true })
        {
            auto* listenParam = processor.apvts.getParameter (ParamIDs::deEssListen);
            REQUIRE (listenParam != nullptr);
            listenParam->setValueNotifyingHost (listenEnabled ? 1.0f : 0.0f);

            TestHelpers::fillWithSine (buffer, 44100.0, 440.0, 0.8f);

            CHECK_NOTHROW (processor.processBlock (buffer, midi));
            CHECK (TestHelpers::allSamplesFinite (buffer));
        }
    }
}

TEST_CASE ("Rapid parameter automation across many blocks produces no NaN/Inf", "[robustness]")
{
    SeraphAudioProcessor processor;
    processor.prepareToPlay (48000.0, 256);

    std::mt19937 rng (1234);
    std::uniform_real_distribution<float> unit (0.0f, 1.0f);

    juce::MidiBuffer midi;

    auto* listenParam = processor.apvts.getParameter (ParamIDs::deEssListen);
    REQUIRE (listenParam != nullptr);

    for (int block = 0; block < 100; ++block)
    {
        setParam (processor, ParamIDs::deEss, unit (rng) * 100.0f);
        setParam (processor, ParamIDs::deEssFreq, 3000.0f + unit (rng) * 9000.0f);
        setParam (processor, ParamIDs::deEssWidth, unit (rng) * 100.0f);
        listenParam->setValueNotifyingHost (unit (rng) > 0.5f ? 1.0f : 0.0f);
        setParam (processor, ParamIDs::air, -6.0f + unit (rng) * 15.0f); // v0.2.0 range (was -12/+12)
        setParam (processor, ParamIDs::comp, unit (rng) * 100.0f);
        setParam (processor, ParamIDs::doubleAmount, unit (rng) * 100.0f);
        setParam (processor, ParamIDs::doubleDetune, unit (rng) * 50.0f);
        setParam (processor, ParamIDs::doubleWidth, unit (rng) * 100.0f);
        setParam (processor, ParamIDs::output, -24.0f + unit (rng) * 48.0f);
        setParam (processor, ParamIDs::mix, unit (rng) * 100.0f);

        juce::AudioBuffer<float> buffer (2, 256);
        TestHelpers::fillWithSine (buffer, 48000.0, 200.0 + unit (rng) * 8000.0, 0.7f);

        CHECK_NOTHROW (processor.processBlock (buffer, midi));
        CHECK (TestHelpers::allSamplesFinite (buffer));
    }
}

TEST_CASE ("reset() followed by processBlock does not crash", "[robustness]")
{
    SeraphAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    setParam (processor, ParamIDs::deEss, 60.0f);
    setParam (processor, ParamIDs::doubleAmount, 60.0f);

    juce::AudioBuffer<float> buffer (2, 512);
    TestHelpers::fillWithSine (buffer, 48000.0, 1000.0, 0.6f);
    juce::MidiBuffer midi;

    processor.processBlock (buffer, midi);

    CHECK_NOTHROW (processor.reset());

    TestHelpers::fillWithSine (buffer, 48000.0, 1000.0, 0.6f);
    CHECK_NOTHROW (processor.processBlock (buffer, midi));
    CHECK (TestHelpers::allSamplesFinite (buffer));
}

TEST_CASE ("NaN/Inf-poisoned input does not propagate silently corrupted output forever", "[robustness]")
{
    // Defensive sweep: feed a block containing NaN/Inf samples (simulating a
    // misbehaving upstream plugin), then verify the processor recovers
    // (produces finite output again) once fed finite input on the next
    // block, rather than latching NaN into internal filter/envelope/delay
    // state forever via reset().
    SeraphAudioProcessor processor;
    processor.prepareToPlay (48000.0, 256);

    auto* deEssParam = processor.apvts.getParameter (ParamIDs::deEss);
    deEssParam->setValueNotifyingHost (deEssParam->convertTo0to1 (60.0f));
    auto* doubleParam = processor.apvts.getParameter (ParamIDs::doubleAmount);
    doubleParam->setValueNotifyingHost (doubleParam->convertTo0to1 (60.0f));

    juce::AudioBuffer<float> poisoned (2, 256);
    poisoned.clear();
    poisoned.setSample (0, 10, std::numeric_limits<float>::quiet_NaN());
    poisoned.setSample (1, 20, std::numeric_limits<float>::infinity());

    juce::MidiBuffer midi;
    CHECK_NOTHROW (processor.processBlock (poisoned, midi));

    processor.reset();

    juce::AudioBuffer<float> finiteAfter (2, 256);
    TestHelpers::fillWithSine (finiteAfter, 48000.0, 1000.0, 0.5f);

    CHECK_NOTHROW (processor.processBlock (finiteAfter, midi));
    CHECK (TestHelpers::allSamplesFinite (finiteAfter));
}
