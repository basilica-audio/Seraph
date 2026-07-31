#include "PluginProcessor.h"
#include "TestHelpers.h"
#include "dsp/SeraphEngine.h"
#include "dsp/SpectralShifter.h"
#include "params/ParameterIds.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <vector>

// Latency contract (SOTA DSP brief ss3.3/ss6.9).
//
// Through v0.2.0 this was a one-line invariant: Seraph reported zero latency
// unconditionally. v0.3.0 lifts that deliberately. Two things can now add
// latency - the doubler's Shift mode (a phase vocoder) and the de-esser's
// lookahead - and both are non-automatable precisely because a latency change
// is something a host should only ever see as a deliberate user action.
//
// What is asserted here is not just the number the plugin reports, but that
// the number is TRUE: a click pushed through the plugin has to come out where
// the reported latency says it will, and at Mix = 50% the dry and wet halves
// of the crossfade have to arrive together.
namespace
{
    void setParam (SeraphAudioProcessor& processor, const char* id, float realValue)
    {
        auto* param = processor.apvts.getParameter (id);
        REQUIRE (param != nullptr);
        param->setValueNotifyingHost (param->convertTo0to1 (realValue));
    }

    // Runs silence through the processor so every smoothed parameter has
    // settled and any mode fade has finished before a measurement is taken.
    void settle (SeraphAudioProcessor& processor, int blockSize, int numBlocks)
    {
        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer midi;

        for (int block = 0; block < numBlocks; ++block)
        {
            buffer.clear();
            processor.processBlock (buffer, midi);
        }
    }

    // Pushes a single band-limited click through the processor and returns
    // the response, so its arrival can be located.
    std::vector<float> renderClick (SeraphAudioProcessor& processor,
                                    int blockSize,
                                    int totalSamples,
                                    int clickPosition)
    {
        std::vector<float> response (static_cast<size_t> (totalSamples), 0.0f);

        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer midi;

        for (int position = 0; position < totalSamples; position += blockSize)
        {
            const auto length = std::min (blockSize, totalSamples - position);
            buffer.clear();

            // A windowed tone burst rather than a bare impulse: an impulse
            // carries energy right up to Nyquist, which the Air shelf and the
            // STFT engine both respond to in ways that smear the arrival and
            // would be measured as latency that is not there.
            for (int sample = 0; sample < length; ++sample)
            {
                const auto offset = position + sample - clickPosition;

                if (offset < 0 || offset >= 64)
                    continue;

                const auto window = 0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * offset / 63.0);
                const auto value = static_cast<float> (
                    window * std::sin (juce::MathConstants<double>::twoPi * 1000.0 * offset / 48000.0));

                buffer.setSample (0, sample, value);
                buffer.setSample (1, sample, value);
            }

            processor.processBlock (buffer, midi);

            for (int sample = 0; sample < length; ++sample)
                response[static_cast<size_t> (position + sample)] = buffer.getSample (0, sample);
        }

        return response;
    }

    // Index of the largest absolute value in a range - where the click landed.
    int peakIndex (const std::vector<float>& signal, int firstIndex, int lastIndex)
    {
        auto peak = std::max (0, firstIndex);

        for (int index = peak; index <= lastIndex && index < static_cast<int> (signal.size()); ++index)
            if (std::abs (signal[static_cast<size_t> (index)]) > std::abs (signal[static_cast<size_t> (peak)]))
                peak = index;

        return peak;
    }
}

TEST_CASE ("Latency is 0 in the default configuration, before and after prepareToPlay", "[latency]")
{
    SeraphAudioProcessor processor;
    CHECK (processor.getLatencySamples() == 0);

    processor.prepareToPlay (48000.0, 512);
    CHECK (processor.getLatencySamples() == 0);
}

TEST_CASE ("Latency stays 0 for every parameter that cannot change it", "[latency]")
{
    SeraphAudioProcessor processor;
    processor.prepareToPlay (44100.0, 256);

    // Everything except doubleMode and deEssLookahead, both of which are
    // non-automatable exactly because they do change it.
    for (const char* id : { ParamIDs::deEss, ParamIDs::deEssWidth, ParamIDs::deEssListen,
                            ParamIDs::deEssLink, ParamIDs::deEssKnee, ParamIDs::air,
                            ParamIDs::airFreq, ParamIDs::comp, ParamIDs::compLink,
                            ParamIDs::doubleAmount, ParamIDs::doubleDetune, ParamIDs::doubleWidth,
                            ParamIDs::doubleHumanize, ParamIDs::doubleFormant })
    {
        auto* param = processor.apvts.getParameter (id);
        REQUIRE (param != nullptr);
        param->setValueNotifyingHost (1.0f);
    }

    settle (processor, 256, 4);

    CHECK (processor.getLatencySamples() == 0);
}

TEST_CASE ("Classic and Micro modes report no latency", "[latency][doubler]")
{
    for (const auto modeIndex : { 0, 1 })
    {
        SeraphAudioProcessor processor;
        processor.prepareToPlay (48000.0, 256);

        auto* mode = processor.apvts.getParameter (ParamIDs::doubleMode);
        REQUIRE (mode != nullptr);
        mode->setValueNotifyingHost (mode->convertTo0to1 (static_cast<float> (modeIndex)));

        settle (processor, 256, 32);

        INFO ("doubleMode index " << modeIndex);
        CHECK (processor.getLatencySamples() == 0);
    }
}

TEST_CASE ("Shift mode reports the STFT engine's latency, inside the brief's budget", "[latency][doubler][shift]")
{
    SeraphAudioProcessor processor;
    processor.prepareToPlay (48000.0, 256);

    auto* mode = processor.apvts.getParameter (ParamIDs::doubleMode);
    REQUIRE (mode != nullptr);
    mode->setValueNotifyingHost (mode->convertTo0to1 (2.0f));

    settle (processor, 256, 32);

    const auto reported = processor.getLatencySamples();

    INFO ("reported " << reported << " samples (" << 1000.0 * reported / 48000.0 << " ms) at 48 kHz");

    CHECK (reported > 0);
    // Binding budget from brief ss3.0: at most 2208 samples (46 ms) at
    // 48 kHz, targeting ~1440 (30 ms).
    CHECK (reported <= 2208);
}

TEST_CASE ("The STFT window is specified in seconds, so it survives a sample-rate change", "[latency][shift]")
{
    // A fixed bin count would halve the physical window at 96 kHz. Configured
    // in seconds, the latency in *milliseconds* stays put while the sample
    // count doubles.
    SpectralShifter at48k;
    at48k.prepare (48000.0, 512);

    SpectralShifter at96k;
    at96k.prepare (96000.0, 512);

    const auto milliseconds48 = 1000.0 * at48k.getLatencySamples() / 48000.0;
    const auto milliseconds96 = 1000.0 * at96k.getLatencySamples() / 96000.0;

    INFO ("48 kHz: " << at48k.getLatencySamples() << " samples (" << milliseconds48 << " ms), "
                     << "96 kHz: " << at96k.getLatencySamples() << " samples (" << milliseconds96 << " ms)");

    CHECK (at96k.getLatencySamples() > at48k.getLatencySamples());
    CHECK (milliseconds96 == Catch::Approx (milliseconds48).margin (2.0));
}

TEST_CASE ("A click arrives exactly where the reported latency says it will", "[latency][doubler][shift]")
{
    constexpr int blockSize = 256;
    constexpr int totalSamples = 48000;
    constexpr int clickPosition = 8192;

    auto measureArrival = [&] (int modeIndex, float lookaheadMs)
    {
        SeraphAudioProcessor processor;
        processor.prepareToPlay (48000.0, blockSize);

        setParam (processor, ParamIDs::doubleAmount, 25.0f);
        setParam (processor, ParamIDs::mix, 100.0f);

        auto* mode = processor.apvts.getParameter (ParamIDs::doubleMode);
        REQUIRE (mode != nullptr);
        mode->setValueNotifyingHost (mode->convertTo0to1 (static_cast<float> (modeIndex)));
        setParam (processor, ParamIDs::deEssLookahead, lookaheadMs);

        settle (processor, blockSize, 64);

        const auto response = renderClick (processor, blockSize, totalSamples, clickPosition);
        const auto reported = processor.getLatencySamples();

        // Searched in a window around the expected arrival: the doubled
        // voices land later still, behind their own base pre-delays, and must
        // not be mistaken for the main path.
        const auto expected = clickPosition + reported;
        const auto arrival = peakIndex (response, expected - 128, expected + 128);

        return std::make_pair (reported, arrival);
    };

    // The measurement is relative to the zero-latency configuration, not
    // absolute: the tone burst's own envelope peaks 35 samples into itself,
    // and that offset is a property of the stimulus, not of the plugin.
    // Differencing against a configuration that reports no latency cancels it
    // and leaves the plugin's own contribution.
    const auto reference = measureArrival (0, 0.0f);
    REQUIRE (reference.first == 0);

    SECTION ("Micro mode adds nothing, as reported")
    {
        const auto measured = measureArrival (1, 0.0f);
        const auto added = measured.second - reference.second;
        INFO ("reported " << measured.first << ", measured " << added);
        CHECK (measured.first == 0);
        CHECK (std::abs (added - measured.first) <= 1);
    }

    SECTION ("Shift mode delays the click by exactly the reported latency")
    {
        const auto measured = measureArrival (2, 0.0f);
        const auto added = measured.second - reference.second;
        INFO ("reported " << measured.first << ", measured " << added);
        CHECK (measured.first > 0);
        CHECK (std::abs (added - measured.first) <= 1);
    }

    SECTION ("Shift mode plus lookahead: still exactly the reported latency")
    {
        const auto measured = measureArrival (2, 2.0f);
        const auto added = measured.second - reference.second;
        INFO ("reported " << measured.first << ", measured " << added);
        CHECK (measured.first > 0);
        CHECK (std::abs (added - measured.first) <= 1);
    }

    SECTION ("lookahead alone delays the click by exactly the reported latency")
    {
        const auto measured = measureArrival (0, 2.0f);
        const auto added = measured.second - reference.second;
        INFO ("reported " << measured.first << ", measured " << added);
        CHECK (measured.first == static_cast<int> (std::lround (0.002 * 48000.0)));
        CHECK (std::abs (added - measured.first) <= 1);
    }
}

TEST_CASE ("De-esser lookahead adds exactly round(ms * sr / 1000) samples", "[latency][deesser][lookahead]")
{
    for (const auto sampleRate : { 44100.0, 48000.0, 96000.0 })
    {
        SeraphAudioProcessor processor;
        processor.prepareToPlay (sampleRate, 256);

        settle (processor, 256, 16);
        const auto baseline = processor.getLatencySamples();

        setParam (processor, ParamIDs::deEssLookahead, 2.0f);
        settle (processor, 256, 16);

        const auto expected = static_cast<int> (std::lround (0.002 * sampleRate));

        INFO ("at " << sampleRate << " Hz: baseline " << baseline << ", with 2 ms lookahead "
                    << processor.getLatencySamples() << ", expected to add " << expected);
        CHECK (processor.getLatencySamples() - baseline == expected);
    }
}

TEST_CASE ("Lookahead and Shift latency add up", "[latency][deesser][doubler][shift]")
{
    SeraphAudioProcessor processor;
    processor.prepareToPlay (48000.0, 256);

    auto* mode = processor.apvts.getParameter (ParamIDs::doubleMode);
    REQUIRE (mode != nullptr);
    mode->setValueNotifyingHost (mode->convertTo0to1 (2.0f));
    settle (processor, 256, 32);

    const auto shiftOnly = processor.getLatencySamples();

    setParam (processor, ParamIDs::deEssLookahead, 2.0f);
    settle (processor, 256, 32);

    CHECK (processor.getLatencySamples() == shiftOnly + static_cast<int> (std::lround (0.002 * 48000.0)));
}

TEST_CASE ("At Mix 50% in Shift mode the dry and wet arrivals stay aligned", "[latency][mix][shift]")
{
    // This is what the dry-path compensation delay exists for: without it the
    // untouched dry capture would arrive a full STFT window ahead of the
    // processed signal, and the crossfade would smear the transient instead
    // of blending it.
    constexpr int blockSize = 256;
    constexpr int totalSamples = 48000;
    constexpr int clickPosition = 8192;

    auto arrivalAtMix = [&] (float mixPercent)
    {
        SeraphAudioProcessor processor;
        processor.prepareToPlay (48000.0, blockSize);

        auto* mode = processor.apvts.getParameter (ParamIDs::doubleMode);
        REQUIRE (mode != nullptr);
        mode->setValueNotifyingHost (mode->convertTo0to1 (2.0f));

        setParam (processor, ParamIDs::mix, mixPercent);
        setParam (processor, ParamIDs::doubleAmount, 0.0f); // isolate the main path from the voices

        settle (processor, blockSize, 64);

        const auto response = renderClick (processor, blockSize, totalSamples, clickPosition);
        const auto expected = clickPosition + processor.getLatencySamples();

        return peakIndex (response, expected - 128, expected + 128) - clickPosition;
    };

    const auto fullyWet = arrivalAtMix (100.0f);
    const auto fullyDry = arrivalAtMix (0.0f);
    const auto halfway = arrivalAtMix (50.0f);

    INFO ("wet " << fullyWet << ", dry " << fullyDry << ", 50% " << halfway);

    // All three land on the same sample: the dry path is delayed by exactly
    // the amount the wet path is.
    CHECK (std::abs (fullyDry - fullyWet) <= 1);
    CHECK (std::abs (halfway - fullyWet) <= 1);
}

TEST_CASE ("SeraphEngine's latency is the sum of its two contributors", "[latency][engine]")
{
    SeraphEngine engine;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = 96000.0;
    spec.maximumBlockSize = 256;
    spec.numChannels = 2;
    engine.prepare (spec);

    CHECK (engine.getLatencySamples() == 0);

    engine.setDeEssLookaheadMs (2.0f);
    const auto lookaheadOnly = engine.getLatencySamples();
    CHECK (lookaheadOnly == static_cast<int> (std::lround (0.002 * 96000.0)));

    engine.setDoubleMode (Doubler::Mode::shift);
    CHECK (engine.getLatencySamples() > lookaheadOnly);

    engine.setDoubleMode (Doubler::Mode::micro);
    CHECK (engine.getLatencySamples() == lookaheadOnly);

    engine.setDeEssLookaheadMs (0.0f);
    CHECK (engine.getLatencySamples() == 0);
}

//==============================================================================
// Suite-wide hardening wave: sample-rate matrix reprepare.
//
// Broader than the fixed-sample-rate latency tests above: this drives one
// processor instance through a full 44.1k -> 96k -> 192k reprepare matrix,
// crossing small AND large block sizes and mono/stereo bus layouts along the
// way, with automation-like parameter churn between reprepares. De-Ess
// Lookahead is deliberately set ONCE, before the loop, and never swept
// during the automation churn inside it - it is documented non-automatable
// (ParameterLayout.cpp: `.withAutomatable (false)`) precisely because it
// changes reported latency, and this test's job is proving prepareToPlay()
// recomputes that reported latency correctly at every sample rate in the
// matrix, not exercising mid-stream automation of a control the plugin
// itself forbids automating. A nonzero, sample-rate-dependent lookahead is
// used (rather than leaving Seraph's own zero-latency default in place) so
// the "correct latency" assertion below is actually exercising the
// round(ms * sr / 1000) recompute (DeEsser.cpp) instead of trivially
// checking "still zero". Deterministic and block counts kept small so this
// stays well under 30s even on Debug/CI.
TEST_CASE ("Sample-rate matrix reprepare: 44.1k -> 96k -> 192k across block sizes and bus "
           "layouts survives parameter automation and reports correct latency every time",
           "[latency][robustness][samplerate][reprepare]")
{
    SeraphAudioProcessor processor;
    juce::MidiBuffer midi;

    constexpr float lookaheadMs = 1.5f;
    setParam (processor, ParamIDs::deEssLookahead, lookaheadMs);

    setParam (processor, ParamIDs::comp, 35.0f);
    setParam (processor, ParamIDs::air, 3.0f);
    setParam (processor, ParamIDs::deEss, 40.0f);
    setParam (processor, ParamIDs::doubleAmount, 30.0f);
    setParam (processor, ParamIDs::output, -2.0f);
    setParam (processor, ParamIDs::mix, 90.0f);

    auto* compParam = processor.apvts.getParameter (ParamIDs::comp);
    REQUIRE (compParam != nullptr);

    // Tracks what Comp's value ought to be at the start of each iteration -
    // seeded from the setParam() above, then updated to the last value the
    // automation loop below left it at, so each reprepare's "did the value
    // survive" check is against ground truth rather than a stale constant.
    auto expectedCompValue = compParam->convertFrom0to1 (compParam->getValue());

    struct Step
    {
        double sampleRate;
        int blockSize;
        int numChannels;
    };

    // Small AND large blocks at both 96k and 192k, plus a mono layout
    // change thrown in at 192k (Seraph supports mono - see
    // isBusesLayoutSupported()) to make sure a channel-count change riding
    // along with a sample-rate reprepare doesn't trip anything up.
    static constexpr Step steps[] = {
        { 44100.0,  32,   2 },
        { 96000.0,  32,   2 },
        { 96000.0,  2048, 2 },
        { 192000.0, 32,   1 },
        { 192000.0, 2048, 2 },
    };

    for (const auto& step : steps)
    {
        if (step.numChannels == 1)
        {
            juce::AudioProcessor::BusesLayout monoLayout;
            monoLayout.inputBuses.add (juce::AudioChannelSet::mono());
            monoLayout.outputBuses.add (juce::AudioChannelSet::mono());
            REQUIRE (processor.setBusesLayout (monoLayout));
        }
        else
        {
            juce::AudioProcessor::BusesLayout stereoLayout;
            stereoLayout.inputBuses.add (juce::AudioChannelSet::stereo());
            stereoLayout.outputBuses.add (juce::AudioChannelSet::stereo());
            REQUIRE (processor.setBusesLayout (stereoLayout));
        }

        processor.prepareToPlay (step.sampleRate, step.blockSize);

        // Ground truth: a standalone engine prepared identically with the
        // same fixed lookahead, so this checks the processor's *reported*
        // latency against what the engine itself computes rather than
        // re-deriving the rounding formula by hand in this test.
        SeraphEngine referenceEngine;
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = step.sampleRate;
        spec.maximumBlockSize = static_cast<juce::uint32> (step.blockSize);
        spec.numChannels = static_cast<juce::uint32> (step.numChannels);
        referenceEngine.prepare (spec);
        referenceEngine.setDeEssLookaheadMs (lookaheadMs);

        // Latency must be reported (and positive - the lookahead always
        // adds some) after every single reprepare in the matrix, not just
        // the first one, and must match the engine's own ground truth.
        CHECK (processor.getLatencySamples() > 0);
        CHECK (processor.getLatencySamples() == referenceEngine.getLatencySamples());

        // State survival: prepareToPlay() must never reset APVTS parameter
        // values, at any sample rate/block-size/layout combination.
        CHECK (compParam->convertFrom0to1 (compParam->getValue())
               == Catch::Approx (expectedCompValue).margin (0.01f));

        juce::AudioBuffer<float> buffer (step.numChannels, step.blockSize);

        for (int block = 0; block < 4; ++block)
        {
            // Automation-like parameter churn while processing, mimicking a
            // host sweeping controls mid-stream between reprepares. Comp/
            // Air/De-Ess/Double are all fully automatable; De-Ess Lookahead
            // and Double Mode are deliberately left untouched here (see the
            // comment above the test).
            const auto sweep = static_cast<float> (block) / 4.0f;
            expectedCompValue = sweep * 100.0f;
            setParam (processor, ParamIDs::comp, expectedCompValue);
            setParam (processor, ParamIDs::air, -6.0f + sweep * 15.0f);
            setParam (processor, ParamIDs::deEss, sweep * 100.0f);
            setParam (processor, ParamIDs::doubleAmount, sweep * 100.0f);

            TestHelpers::fillWithSine (buffer, step.sampleRate, 300.0, 0.6f,
                                       static_cast<juce::int64> (block) * step.blockSize);

            CHECK_NOTHROW (processor.processBlock (buffer, midi));
            CHECK (TestHelpers::allSamplesFinite (buffer));
        }
    }
}
