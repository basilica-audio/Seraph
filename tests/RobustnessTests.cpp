#include "PluginProcessor.h"
#include "dsp/MicroPitchShifter.h"
#include "dsp/SpectralShifter.h"
#include "params/ParameterIds.h"
#include "TestHelpers.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <limits>
#include <vector>
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

//==============================================================================
// v0.3.0 robustness for the new engines (SOTA DSP brief ss6.16).
namespace
{
    void selectMode (SeraphAudioProcessor& processor, int modeIndex)
    {
        auto* mode = processor.apvts.getParameter (ParamIDs::doubleMode);
        REQUIRE (mode != nullptr);
        mode->setValueNotifyingHost (mode->convertTo0to1 (static_cast<float> (modeIndex)));
    }

    const char* modeName (int modeIndex)
    {
        return modeIndex == 0 ? "Classic" : modeIndex == 1 ? "Micro" : "Shift";
    }
}

TEST_CASE ("NaN/Inf injection recovers after reset() in every doubler mode", "[robustness][doubler]")
{
    // Same contract the pre-existing poisoned-input test above establishes,
    // extended to the two new engines: a poisoned block must not crash, and
    // reset() must return the chain to finite output.
    //
    // Self-healing without reset() is deliberately NOT claimed. One-pole
    // envelope followers and IIR filters latch NaN by construction - once a
    // state variable is NaN, every subsequent multiply-accumulate keeps it
    // NaN - so recovering without clearing state would need a finiteness
    // guard on every filter in the chain, including the ones inside
    // juce::dsp. reset() is the documented remedy, and hosts call it on
    // transport changes. What the new engines must not do is make this worse,
    // which is what the per-mode sweep below checks.
    for (const auto modeIndex : { 0, 1, 2 })
    {
        SeraphAudioProcessor processor;
        processor.prepareToPlay (48000.0, 256);

        selectMode (processor, modeIndex);
        setParam (processor, ParamIDs::doubleAmount, 90.0f);
        setParam (processor, ParamIDs::doubleDetune, 30.0f);
        setParam (processor, ParamIDs::doubleHumanize, 50.0f);
        setParam (processor, ParamIDs::deEss, 80.0f);
        setParam (processor, ParamIDs::deEssLookahead, 2.0f);

        juce::AudioBuffer<float> buffer (2, 256);
        juce::MidiBuffer midi;

        for (int block = 0; block < 8; ++block)
        {
            TestHelpers::fillWithSine (buffer, 48000.0, 1000.0, 0.5f, static_cast<juce::int64> (block) * 256);
            processor.processBlock (buffer, midi);
        }

        TestHelpers::fillWithSine (buffer, 48000.0, 1000.0, 0.5f);
        buffer.setSample (0, 10, std::numeric_limits<float>::quiet_NaN());
        buffer.setSample (1, 20, std::numeric_limits<float>::infinity());
        buffer.setSample (0, 30, -std::numeric_limits<float>::infinity());

        INFO ("doubler mode " << modeName (modeIndex));
        CHECK_NOTHROW (processor.processBlock (buffer, midi));

        processor.reset();

        // Enough blocks for the Shift mode's STFT window to refill with clean
        // audio after the reset.
        bool recovered = false;

        for (int block = 0; block < 64 && ! recovered; ++block)
        {
            TestHelpers::fillWithSine (buffer, 48000.0, 1000.0, 0.5f, static_cast<juce::int64> (block) * 256);
            processor.processBlock (buffer, midi);
            recovered = TestHelpers::allSamplesFinite (buffer);
        }

        CHECK (recovered);
    }
}

TEST_CASE ("The new detune engines handle poisoned samples", "[robustness][doubler]")
{
    // Asserted at the unit level, where no latching filter from the
    // surrounding chain sits in the way, so each engine's own behaviour is
    // visible.
    //
    // The two get there differently. MicroPitchShifter is a delay line, which
    // holds audio and nothing else, so it clocks the poison straight out.
    // SpectralShifter wraps a phase vocoder, whose per-bin phase accumulators
    // latch NaN permanently - measurably not even its own reset() clears
    // them - so the wrapper substitutes silence for non-finite input before
    // the engine ever sees it.
    constexpr double sampleRate = 48000.0;

    SECTION ("MicroPitchShifter flushes it out on its own")
    {
        MicroPitchShifter shifter;
        shifter.prepare (sampleRate, 9.0f);
        shifter.setDetuneCents (20.0f);

        for (int sample = 0; sample < 4800; ++sample)
            shifter.processSample (0.4f);

        shifter.processSample (std::numeric_limits<float>::quiet_NaN());
        shifter.processSample (std::numeric_limits<float>::infinity());

        // Longer than the shifter's own delay-line capacity, so every stored
        // copy of the poison has been overwritten and read past.
        bool recovered = false;

        for (int sample = 0; sample < 96000; ++sample)
            recovered = std::isfinite (shifter.processSample (0.4f));

        CHECK (recovered);
    }

    SECTION ("SpectralShifter never lets poison reach the engine")
    {
        SpectralShifter shifter;
        shifter.prepare (sampleRate, 512);
        shifter.setDetuneCents (20.0f);

        std::vector<float> input (512, 0.0f);
        std::vector<float> output (512, 0.0f);

        for (int sample = 0; sample < 512; ++sample)
            input[static_cast<size_t> (sample)] = 0.4f * std::sin (0.05 * sample);

        auto outputIsFinite = [&output]
        {
            return std::all_of (output.begin(), output.end(),
                                [] (float value) { return std::isfinite (value); });
        };

        for (int block = 0; block < 16; ++block)
            shifter.process (input.data(), output.data(), 512);

        REQUIRE (outputIsFinite());

        auto poisoned = input;
        poisoned[10] = std::numeric_limits<float>::quiet_NaN();
        poisoned[20] = std::numeric_limits<float>::infinity();
        poisoned[30] = -std::numeric_limits<float>::infinity();

        shifter.process (poisoned.data(), output.data(), 512);

        // Not "recovers eventually" - the output stays finite through the
        // poisoned block itself and every block after it.
        CHECK (outputIsFinite());

        for (int block = 0; block < 32; ++block)
        {
            shifter.process (input.data(), output.data(), 512);
            REQUIRE (outputIsFinite());
        }
    }
}

TEST_CASE ("Long silence after signal does not degrade block timing (denormals)", "[robustness][denormal]")
{
    // Denormal arithmetic is orders of magnitude slower on some hardware, and
    // a chain full of decaying IIR state and delay lines is exactly where it
    // shows up. ScopedNoDenormals in processBlock() is what prevents it; this
    // measures that the protection is actually in force.
    SeraphAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    setParam (processor, ParamIDs::deEss, 70.0f);
    setParam (processor, ParamIDs::comp, 70.0f);
    setParam (processor, ParamIDs::doubleAmount, 80.0f);
    selectMode (processor, 1); // Micro: long delay lines, always running

    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;

    constexpr int measuredBlocks = 200;

    // Loud programme material first, timed.
    const auto loudStart = juce::Time::getHighResolutionTicks();

    for (int block = 0; block < measuredBlocks; ++block)
    {
        TestHelpers::fillWithSine (buffer, 48000.0, 440.0, 0.5f, static_cast<juce::int64> (block) * 512);
        processor.processBlock (buffer, midi);
    }

    const auto loudSeconds = juce::Time::highResolutionTicksToSeconds (
        juce::Time::getHighResolutionTicks() - loudStart);

    // Then ten seconds of digital silence, so every filter state decays
    // towards the denormal range.
    for (int block = 0; block < 10 * 48000 / 512; ++block)
    {
        buffer.clear();
        processor.processBlock (buffer, midi);
    }

    const auto silentStart = juce::Time::getHighResolutionTicks();

    for (int block = 0; block < measuredBlocks; ++block)
    {
        buffer.clear();
        processor.processBlock (buffer, midi);
    }

    const auto silentSeconds = juce::Time::highResolutionTicksToSeconds (
        juce::Time::getHighResolutionTicks() - silentStart);

    const auto ratio = silentSeconds / std::max (1.0e-9, loudSeconds);

    INFO ("loud " << loudSeconds * 1000.0 << " ms, silent " << silentSeconds * 1000.0 << " ms, ratio " << ratio);

    // Processing silence must not become measurably *slower* than processing
    // signal. A generous bound: this is a timing test on a shared CI machine,
    // and the failure it guards against (denormals unprotected) is a
    // 10x-100x effect, not a 30% one.
    CHECK (ratio < 3.0);
}

TEST_CASE ("Oversized blocks are handled safely in every doubler mode", "[robustness][doubler]")
{
    // A host that hands over a larger block than prepareToPlay() was told
    // about must not cause a buffer overrun. The engines clamp to their
    // prepared scratch size and leave the overflow tail unprocessed.
    for (const auto modeIndex : { 0, 1, 2 })
    {
        SeraphAudioProcessor processor;
        processor.prepareToPlay (48000.0, 256);

        selectMode (processor, modeIndex);
        setParam (processor, ParamIDs::doubleAmount, 80.0f);
        setParam (processor, ParamIDs::deEssLookahead, 2.0f);

        juce::AudioBuffer<float> oversized (2, 4096);
        juce::MidiBuffer midi;
        TestHelpers::fillWithSine (oversized, 48000.0, 1000.0, 0.5f);

        processor.processBlock (oversized, midi);

        INFO ("doubler mode " << modeName (modeIndex));
        CHECK (TestHelpers::allSamplesFinite (oversized));
        CHECK (TestHelpers::peakAbsolute (oversized) < 100.0f);
    }
}

TEST_CASE ("Rapid mode switching under load stays finite and bounded", "[robustness][doubler]")
{
    SeraphAudioProcessor processor;
    processor.prepareToPlay (48000.0, 256);

    setParam (processor, ParamIDs::doubleAmount, 90.0f);
    setParam (processor, ParamIDs::doubleDetune, 35.0f);
    setParam (processor, ParamIDs::doubleHumanize, 60.0f);

    juce::AudioBuffer<float> buffer (2, 256);
    juce::MidiBuffer midi;

    for (int block = 0; block < 600; ++block)
    {
        // Switch faster than the 10 ms fade can finish, so a switch is
        // frequently interrupted by the next one.
        selectMode (processor, block % 3);

        TestHelpers::fillWithSine (buffer, 48000.0, 660.0, 0.6f, static_cast<juce::int64> (block) * 256);
        processor.processBlock (buffer, midi);

        REQUIRE (TestHelpers::allSamplesFinite (buffer));
        REQUIRE (TestHelpers::peakAbsolute (buffer) < 10.0f);
    }
}

TEST_CASE ("Every doubler mode survives the documented sample-rate range", "[robustness][doubler][samplerate]")
{
    for (const auto sampleRate : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        for (const auto modeIndex : { 0, 1, 2 })
        {
            SeraphAudioProcessor processor;
            processor.prepareToPlay (sampleRate, 256);

            selectMode (processor, modeIndex);
            setParam (processor, ParamIDs::doubleAmount, 100.0f);
            setParam (processor, ParamIDs::doubleDetune, 50.0f);
            setParam (processor, ParamIDs::doubleHumanize, 100.0f);
            setParam (processor, ParamIDs::deEss, 100.0f);
            setParam (processor, ParamIDs::deEssLookahead, 2.0f);
            setParam (processor, ParamIDs::comp, 100.0f);

            juce::AudioBuffer<float> buffer (2, 256);
            juce::MidiBuffer midi;

            for (int block = 0; block < 64; ++block)
            {
                TestHelpers::fillWithSine (buffer, sampleRate, 1000.0, 0.7f,
                                           static_cast<juce::int64> (block) * 256);
                processor.processBlock (buffer, midi);
            }

            INFO ("sample rate " << sampleRate << ", doubler mode " << modeName (modeIndex));
            CHECK (TestHelpers::allSamplesFinite (buffer));
            CHECK (TestHelpers::peakAbsolute (buffer) < 20.0f);
        }
    }
}
